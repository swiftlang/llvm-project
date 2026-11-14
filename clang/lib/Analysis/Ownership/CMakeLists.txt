add_clang_library(clangAnalysisOwnership
  SelfContainment.cpp

  LINK_LIBS
  clangAST
  clangAnalysis
  clangAnalysisLifetimeSafety
  clangBasic
  )
