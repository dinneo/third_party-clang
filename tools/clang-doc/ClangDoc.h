//===-- ClangDoc.cpp - ClangDoc ---------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_DOC_CLANGDOC_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_DOC_CLANGDOC_H

#include "ClangDocReporter.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Tooling.h"
#include <string>
#include <vector>

namespace clang {
namespace doc {

// A Context which contains extra options which are used in ClangMoveTool.
struct ClangDocContext {
  // Which format to emit representation in.
  std::string EmitFormat;
};

class ClangDocVisitor : public RecursiveASTVisitor<ClangDocVisitor> {
public:
  explicit ClangDocVisitor(ASTContext *Context, ClangDocReporter &Reporter)
      : Context(Context), Reporter(Reporter) {}

  virtual bool VisitNamedDecl(const NamedDecl *D);

  void ParseUnattachedComments();
  bool IsNewComment(SourceLocation Loc, SourceManager &Manager) const;

private:
  ASTContext *Context;
  ClangDocReporter &Reporter;
};

class ClangDocConsumer : public clang::ASTConsumer {
public:
  explicit ClangDocConsumer(ASTContext *Context, ClangDocReporter &Reporter)
      : Visitor(Context, Reporter), Reporter(Reporter) {}

  virtual void HandleTranslationUnit(clang::ASTContext &Context);

private:
  ClangDocVisitor Visitor;
  ClangDocReporter &Reporter;
};

class ClangDocAction : public clang::ASTFrontendAction {
public:
  ClangDocAction(ClangDocReporter &Reporter) : Reporter(Reporter) {}

  virtual std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &Compiler, llvm::StringRef InFile);
  virtual void EndSourceFileAction();

private:
  ClangDocReporter &Reporter;
};

class ClangDocActionFactory : public tooling::FrontendActionFactory {
public:
  ClangDocActionFactory(ClangDocContext &Context, ClangDocReporter &Reporter)
      : Context(Context), Reporter(Reporter) {}

  clang::FrontendAction *create() override {
    return new ClangDocAction(Reporter);
  }

private:
  ClangDocContext &Context;
  ClangDocReporter &Reporter;
};

} // namespace doc
} // namespace clang

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_DOC_CLANGDOC_H
