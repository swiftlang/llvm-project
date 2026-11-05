add_clang_library(clangAnalysisLifetimeSafety
  Checker.cpp
  Facts.cpp
  FactsGenerator.cpp
  LifetimeAnnotations.cpp
  LifetimeInference.cpp
  LifetimeSafety.cpp
  LiveOrigins.cpp
  LifetimeStats.cpp
  LifetimeSummary.cpp
  Loans.cpp
  LoanPropagation.cpp
  MovedLoans.cpp
  Origins.cpp

  LINK_LIBS
  clangAST
  clangAnalysis
  clangBasic
  )

