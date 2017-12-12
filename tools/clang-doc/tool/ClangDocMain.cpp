//===-- ClangDocMain.cpp - Clangdoc -----------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "ClangDoc.h"
#include "clang/Driver/Options.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Signals.h"
#include <string>

using namespace clang;
using namespace llvm;

namespace {

cl::OptionCategory ClangDocCategory("clang-doc options");

cl::opt<std::string>
    EmitFormat("emit",
               cl::desc("Output format (valid options are -emit=json and "
                        "-emit=llvm)."),
               cl::init("llvm"), cl::cat(ClangDocCategory));

cl::opt<bool>
    DoxygenOnly("doxygen",
                cl::desc("Use only doxygen-style comments to generate docs."),
                cl::init(false), cl::cat(ClangDocCategory));

} // namespace

int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  tooling::CommonOptionsParser OptionsParser(argc, argv, ClangDocCategory);

  if (EmitFormat != "llvm" && EmitFormat != "json") {
    llvm::errs() << "Please specify llvm or json output.\n";
    return 1;
  }

  // TODO: Update the source path list to only consider changed files for
  // incremental doc updates
  doc::ClangDocReporter Reporter(OptionsParser.getSourcePathList());
  doc::ClangDocContext Context{EmitFormat};
  llvm::outs() << "Output results in " << Context.EmitFormat << " format.\n";

  tooling::ClangTool Tool(OptionsParser.getCompilations(),
                          OptionsParser.getSourcePathList());

  if (!DoxygenOnly)
    Tool.appendArgumentsAdjuster(tooling::getInsertArgumentAdjuster(
        "-fparse-all-comments", tooling::ArgumentInsertPosition::BEGIN));

  doc::ClangDocActionFactory Factory(Context, Reporter);

  llvm::outs() << "Parsing codebase...\n";
  int Status = Tool.run(&Factory);
  if (Status)
    return Status;

  llvm::outs() << "Writing docs...\n";
  Reporter.Serialize(EmitFormat, llvm::outs());
}
