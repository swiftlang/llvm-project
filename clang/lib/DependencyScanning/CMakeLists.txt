set(LLVM_LINK_COMPONENTS
  Core
  Option
  Support
  TargetParser
  CAS
  )

add_clang_library(clangDependencyScanning
  DependencyGraph.cpp
  DependencyScanningFilesystem.cpp
  DependencyScanningService.cpp
  DependencyScanningWorker.cpp
  IncludeTreeActionController.cpp
  DependencyScanningUtils.cpp
  InProcessModuleCache.cpp
  ModuleDepCollector.cpp
  ScanAndUpdateArgs.cpp

  DEPENDS
  ClangDriverOptions

  LINK_LIBS
  clangAST
  clangBasic
  clangCAS
  clangFrontend
  clangLex
  clangSerialization
  ${LLVM_PTHREAD_LIB}
  )
