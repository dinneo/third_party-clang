//===-- Doc.cpp - ClangDoc --------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_DOC_CLANG_DOC_REPORTER_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_DOC_CLANG_DOC_REPORTER_H

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/CommentVisitor.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/raw_ostream.h"
#include <set>
#include <string>
#include <vector>

using namespace clang::comments;

namespace clang {
namespace doc {

struct StringPair {
  std::string Key;
  std::string Value;
};

struct CommentInfo {
  std::string Kind;
  std::string Text;
  std::string Name;
  std::string Direction;
  std::string ParamName;
  std::string CloseName;
  bool SelfClosing = false;
  bool Explicit = false;
  std::vector<std::string> Args;
  std::vector<StringPair> Attrs;
  std::vector<int> Position;
  std::vector<CommentInfo> Children;
};

// TODO: collect declarations of the same object, comment is preferentially:
// 1) docstring on definition, 2) combined docstring from non-def decls, or
// 3) comment on definition, 4) no comment.
struct DeclInfo {
  const Decl *D;
  std::string QualifiedName;
  CommentInfo Comment;
};

struct FileRecord {
  std::string Filename;
  std::vector<DeclInfo> Decls;
  std::vector<CommentInfo> UnattachedComments;
};

class ClangDocReporter : public ConstCommentVisitor<ClangDocReporter> {
public:
  ClangDocReporter(std::vector<std::string> SourcePathList);

  void AddComment(StringRef Filename, CommentInfo &CI);
  void AddDecl(StringRef Filename, DeclInfo &D);
  void AddFile(StringRef Filename);
  void AddFileInTU(StringRef Filename) { FilesInThisTU.insert(Filename); }
  void AddFileSeen(StringRef Filename) { FilesSeen.insert(Filename); }
  void ClearFilesInThisTU() { FilesInThisTU.clear(); };

  void visitTextComment(const TextComment *C);
  void visitInlineCommandComment(const InlineCommandComment *C);
  void visitHTMLStartTagComment(const HTMLStartTagComment *C);
  void visitHTMLEndTagComment(const HTMLEndTagComment *C);
  void visitBlockCommandComment(const BlockCommandComment *C);
  void visitParamCommandComment(const ParamCommandComment *C);
  void visitTParamCommandComment(const TParamCommandComment *C);
  void visitVerbatimBlockComment(const VerbatimBlockComment *C);
  void visitVerbatimBlockLineComment(const VerbatimBlockLineComment *C);
  void visitVerbatimLineComment(const VerbatimLineComment *C);

  CommentInfo ParseFullComment(comments::FullComment *Comment);

  std::set<std::string> GetFilesInThisTU() const { return FilesInThisTU; }
  bool HasFile(StringRef Filename) const;
  bool HasSeenFile(StringRef Filename) const;
  void Serialize(StringRef Format, llvm::raw_ostream &OS) const;

private:
  void parseComment(CommentInfo *CI, comments::Comment *C);
  void serializeYAML(llvm::raw_ostream &OS) const;
  void serializeLLVM(llvm::raw_ostream &OS) const;
  const char *getCommandName(unsigned CommandID);
  bool isWhitespaceOnly(std::string S);

  CommentInfo *CurrentCI;
  llvm::StringMap<FileRecord> FileRecords;
  std::set<std::string> FilesInThisTU;
  std::set<std::string> FilesSeen;
};

} // namespace doc
} // namespace clang

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_DOC_CLANG_DOC_REPORTER_H
