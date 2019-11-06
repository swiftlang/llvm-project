if (LLDB_ENABLE_SWIFT_SUPPORT)
  set(LLDB_EXPRESSION_TEST_SWIFT_SOURCES
    SwiftParserTest.cpp
  )
endif()

add_lldb_unittest(ExpressionTests
  ClangParserTest.cpp
  DiagnosticManagerTest.cpp
  DWARFExpressionTest.cpp
  ${LLDB_EXPRESSION_TEST_SWIFT_SOURCES}
  CppModuleConfigurationTest.cpp

  LINK_LIBS
    lldbCore
    lldbPluginExpressionParserClang
    lldbUtility
    lldbUtilityHelpers
    LLVMTestingSupport
  )
