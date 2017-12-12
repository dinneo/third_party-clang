set(LLVM_LINK_COMPONENTS
  support
  )

add_clang_library(clangDoc
  ClangDoc.cpp
  ClangDocReporter.cpp

  LINK_LIBS
  clangAnalysis
  clangAST
  clangASTMatchers
  clangBasic
  clangFormat
  clangFrontend
  clangLex
  clangTooling
  clangToolingCore
  )

add_subdirectory(tool)
