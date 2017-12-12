include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

add_clang_executable(clang-doc
  ClangDocMain.cpp
  )

target_link_libraries(clang-doc
  clangAST
  clangASTMatchers
  clangBasic
  clangFormat
  clangFrontend
  clangDoc
  clangRewrite
  clangTooling
  clangToolingCore
  )