if(LLDB_ENABLE_SWIFT_SUPPORT)
  set(LLDB_SYMBOL_TEST_SWIFT_SOURCES
    TestSwiftASTContext.cpp
  )
endif()

add_lldb_unittest(SymbolTests
  LocateSymbolFileTest.cpp
  PostfixExpressionTest.cpp
  TestClangASTContext.cpp
  TestDWARFCallFrameInfo.cpp
  TestType.cpp
  ${LLDB_SYMBOL_TEST_SWIFT_SOURCES}
  TestLineEntry.cpp

  LINK_LIBS
    lldbHost
    lldbSymbol
    lldbUtilityHelpers
    lldbPluginObjectFileELF
    lldbPluginObjectFileMachO
    lldbPluginSymbolFileDWARF
    lldbPluginSymbolFileSymtab
    LLVMTestingSupport
  )

set(test_inputs
  inlined-functions.yaml
  )
add_unittest_inputs(SymbolTests "${test_inputs}")
