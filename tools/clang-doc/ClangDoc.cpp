//===-- ClangDoc.cpp - ClangDoc ---------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "ClangDoc.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Comment.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

namespace clang {
namespace doc {

// TODO: limit to functions/objects/namespaces/etc?
bool ClangDocVisitor::VisitNamedDecl(const NamedDecl *D) {
  SourceManager &Manager = Context->getSourceManager();
  if (!IsNewComment(D->getLocation(), Manager))
    return true;

  DeclInfo DI;
  DI.D = D;
  DI.QualifiedName = D->getQualifiedNameAsString();
  RawComment *Comment = Context->getRawCommentForDeclNoCache(D);

  // TODO: Move set attached to the initial comment parsing, not here
  if (Comment) {
    Comment->setAttached();
    DI.Comment =
        Reporter.ParseFullComment(Comment->parse(*Context, nullptr, D));
  }
  Reporter.AddDecl(Manager.getFilename(D->getLocation()), DI);
  return true;
}

void ClangDocVisitor::ParseUnattachedComments() {
  SourceManager &Manager = Context->getSourceManager();
  for (RawComment *Comment : Context->getRawCommentList().getComments()) {
    if (!IsNewComment(Comment->getLocStart(), Manager) || Comment->isAttached())
      continue;
    CommentInfo CI =
        Reporter.ParseFullComment(Comment->parse(*Context, nullptr, nullptr));
    Reporter.AddComment(Manager.getFilename(Comment->getLocStart()), CI);
  }
}

bool ClangDocVisitor::IsNewComment(SourceLocation Loc,
                                   SourceManager &Manager) const {
  if (!Loc.isValid())
    return false;
  const std::string &Filename = Manager.getFilename(Loc);
  if (!Reporter.HasFile(Filename) || Reporter.HasSeenFile(Filename))
    return false;
  if (Manager.isInSystemHeader(Loc) || Manager.isInExternCSystemHeader(Loc))
    return false;
  Reporter.AddFileInTU(Filename);
  return true;
}

void ClangDocConsumer::HandleTranslationUnit(ASTContext &Context) {
  Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  Visitor.ParseUnattachedComments();
}

std::unique_ptr<ASTConsumer>
ClangDocAction::CreateASTConsumer(CompilerInstance &Compiler,
                                  llvm::StringRef InFile) {
  return std::unique_ptr<ASTConsumer>(
      new ClangDocConsumer(&Compiler.getASTContext(), Reporter));
}

void ClangDocAction::EndSourceFileAction() {
  for (const auto &Filename : Reporter.GetFilesInThisTU()) {
    Reporter.AddFileSeen(Filename);
  }
  Reporter.ClearFilesInThisTU();
}

} // namespace doc
} // namespace clang
