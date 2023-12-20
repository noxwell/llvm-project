set(LLVM_LINK_COMPONENTS
  Support
  )

add_clang_executable(callsite-wrapper
  CallsiteWrapper.cpp
  )

target_link_libraries(callsite-wrapper
  PRIVATE
  clangAST
  clangASTMatchers
  clangBasic
  clangFrontend
  clangRewrite
  clangSerialization
  clangTooling
  clangToolingRefactoring
  )
