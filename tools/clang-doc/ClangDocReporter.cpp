//===-- Doc.cpp - ClangDoc --------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "ClangDocReporter.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/CommentVisitor.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/YAMLTraits.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

LLVM_YAML_IS_SEQUENCE_VECTOR(clang::doc::DeclInfo)
LLVM_YAML_IS_SEQUENCE_VECTOR(clang::doc::StringPair)
LLVM_YAML_IS_SEQUENCE_VECTOR(clang::doc::CommentInfo)

namespace llvm {
namespace yaml {

template <> struct MappingTraits<clang::doc::StringPair> {
  static void mapping(IO &IO, clang::doc::StringPair &Info) {
    IO.mapRequired("key", Info.Key);
    IO.mapRequired("value", Info.Value);
  }
};

template <> struct MappingTraits<clang::doc::FileRecord> {
  static void mapping(IO &IO, clang::doc::FileRecord &Info) {
    IO.mapRequired("Filename", Info.Filename);
    IO.mapRequired("Decls", Info.Decls);
    IO.mapRequired("UnattachedComments", Info.UnattachedComments);
  }
};

template <> struct MappingTraits<clang::doc::DeclInfo> {
  static void mapping(IO &IO, clang::doc::DeclInfo &Info) {
    IO.mapRequired("Name", Info.QualifiedName);
    IO.mapRequired("Comment", Info.Comment);
  }
};

template <> struct MappingTraits<clang::doc::CommentInfo> {
  static void mapping(IO &IO, clang::doc::CommentInfo &Info) {
    IO.mapRequired("Kind", Info.Kind);
    if (!Info.Text.empty())
      IO.mapRequired("Text", Info.Text);
    if (!Info.Name.empty())
      IO.mapRequired("Text", Info.Name);
    if (!Info.Direction.empty())
      IO.mapRequired("Direction", Info.Direction);
    if (!Info.ParamName.empty())
      IO.mapRequired("ParamName", Info.ParamName);
    if (!Info.CloseName.empty())
      IO.mapRequired("CloseName", Info.CloseName);
    if (Info.SelfClosing)
      IO.mapRequired("SelfClosing", Info.SelfClosing);
    if (Info.Explicit)
      IO.mapRequired("Explicit", Info.Explicit);
    if (Info.Args.size() > 0)
      IO.mapRequired("Args", Info.Args);
    if (Info.Attrs.size() > 0)
      IO.mapRequired("Attrs", Info.Attrs);
    if (Info.Position.size() > 0)
      IO.mapRequired("Position", Info.Position);
    if (Info.Children.size() > 0)
      IO.mapRequired("Children", Info.Children);
  }
};

} // end namespace yaml
} // end namespace llvm

namespace clang {
namespace doc {

ClangDocReporter::ClangDocReporter(std::vector<std::string> SourcePathList) {
  for (std::string Path : SourcePathList)
    AddFile(Path);
}

void ClangDocReporter::AddComment(StringRef Filename, CommentInfo &CI) {
  FileRecords[Filename].UnattachedComments.push_back(std::move(CI));
}

void ClangDocReporter::AddDecl(StringRef Filename, DeclInfo &DI) {
  FileRecords[Filename].Decls.push_back(std::move(DI));
}

void ClangDocReporter::AddFile(StringRef Filename) {
  FileRecord FI;
  FI.Filename = Filename;
  FileRecords.insert(std::pair<StringRef, FileRecord>(Filename, std::move(FI)));
}

CommentInfo ClangDocReporter::ParseFullComment(comments::FullComment *Comment) {
  CommentInfo CI;
  parseComment(&CI, Comment);
  return CI;
}

void ClangDocReporter::visitTextComment(const TextComment *C) {
  if (!isWhitespaceOnly(C->getText()))
    CurrentCI->Text = C->getText();
}

void ClangDocReporter::visitInlineCommandComment(
    const InlineCommandComment *C) {
  CurrentCI->Name = getCommandName(C->getCommandID());
  for (unsigned i = 0, e = C->getNumArgs(); i != e; ++i)
    CurrentCI->Args.push_back(C->getArgText(i));
}

void ClangDocReporter::visitHTMLStartTagComment(const HTMLStartTagComment *C) {
  CurrentCI->Name = C->getTagName();
  if (C->getNumAttrs() != 0) {
    for (unsigned i = 0, e = C->getNumAttrs(); i != e; ++i) {
      const HTMLStartTagComment::Attribute &Attr = C->getAttr(i);
      StringPair Pair{Attr.Name, Attr.Value};
      CurrentCI->Attrs.push_back(std::move(Pair));
    }
  }
  C->isSelfClosing() ? CurrentCI->SelfClosing = true
                     : CurrentCI->SelfClosing = false;
}

void ClangDocReporter::visitHTMLEndTagComment(const HTMLEndTagComment *C) {
  CurrentCI->Name = C->getTagName();
  CurrentCI->SelfClosing = true;
}

void ClangDocReporter::visitBlockCommandComment(const BlockCommandComment *C) {
  CurrentCI->Name = getCommandName(C->getCommandID());
  for (unsigned i = 0, e = C->getNumArgs(); i != e; ++i)
    CurrentCI->Args.push_back(C->getArgText(i));
}

void ClangDocReporter::visitParamCommandComment(const ParamCommandComment *C) {
  CurrentCI->Direction =
      ParamCommandComment::getDirectionAsString(C->getDirection());
  C->isDirectionExplicit() ? CurrentCI->Explicit = true
                           : CurrentCI->Explicit = false;
  if (C->hasParamName() && C->isParamIndexValid())
    CurrentCI->ParamName = C->getParamNameAsWritten();
}

void ClangDocReporter::visitTParamCommandComment(
    const TParamCommandComment *C) {
  if (C->hasParamName() && C->isPositionValid())
    CurrentCI->ParamName = C->getParamNameAsWritten();

  if (C->isPositionValid()) {
    for (unsigned i = 0, e = C->getDepth(); i != e; ++i)
      CurrentCI->Position.push_back(C->getIndex(i));
  }
}

void ClangDocReporter::visitVerbatimBlockComment(
    const VerbatimBlockComment *C) {
  CurrentCI->Name = getCommandName(C->getCommandID());
  CurrentCI->CloseName = C->getCloseName();
}

void ClangDocReporter::visitVerbatimBlockLineComment(
    const VerbatimBlockLineComment *C) {
  if (!isWhitespaceOnly(C->getText()))
    CurrentCI->Text = C->getText();
}

void ClangDocReporter::visitVerbatimLineComment(const VerbatimLineComment *C) {
  if (!isWhitespaceOnly(C->getText()))
    CurrentCI->Text = C->getText();
}

bool ClangDocReporter::HasFile(StringRef Filename) const {
  return FileRecords.find(Filename) != FileRecords.end();
}

bool ClangDocReporter::HasSeenFile(StringRef Filename) const {
  return FilesSeen.find(Filename) != FilesSeen.end();
}

void ClangDocReporter::Serialize(StringRef Format,
                                 llvm::raw_ostream &OS) const {
  Format == "llvm" ? serializeLLVM(OS) : serializeYAML(OS);
}

void ClangDocReporter::parseComment(CommentInfo *CI, comments::Comment *C) {
  CurrentCI = CI;
  CI->Kind = C->getCommentKindName();
  ConstCommentVisitor<ClangDocReporter>::visit(C);
  for (comments::Comment::child_iterator I = C->child_begin(),
                                         E = C->child_end();
       I != E; ++I) {
    CommentInfo ChildCI;
    parseComment(&ChildCI, *I);
    CI->Children.push_back(std::move(ChildCI));
  }
}

void ClangDocReporter::serializeYAML(llvm::raw_ostream &OS) const {
  yaml::Output Output(OS);
  for (const auto &F : FileRecords) {
    FileRecord NonConstValue = F.second;
    Output << NonConstValue;
  }
}

void ClangDocReporter::serializeLLVM(llvm::raw_ostream &OS) const {
  // TODO: Implement.
  OS << "Not yet implemented.\n";
}

const char *ClangDocReporter::getCommandName(unsigned CommandID) {
  ;
  const CommandInfo *Info = CommandTraits::getBuiltinCommandInfo(CommandID);
  if (Info)
    return Info->Name;
  // TODO: Add parsing for \file command.
  return "<not a builtin command>";
}

bool ClangDocReporter::isWhitespaceOnly(std::string S) {
  return S.find_first_not_of(" \t\n\v\f\r") == std::string::npos || S.empty();
}

} // namespace doc
} // namespace clang
