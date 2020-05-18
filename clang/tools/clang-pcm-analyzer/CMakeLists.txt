set(LLVM_LINK_COMPONENTS
  BitstreamReader
  Core
  Support
  )

add_clang_tool(clang-pcm-analyzer
  ClangPcmAnalyzer.cpp
  )

set(CLANG_SCAN_DEPS_LIB_DEPS
  clangBasic
  clangSerialization
  )

clang_target_link_libraries(clang-pcm-analyzer
  PRIVATE
  ${CLANG_SCAN_DEPS_LIB_DEPS}
  )
