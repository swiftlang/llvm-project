set(SWIFT_SOURCES
  SwiftDemanglingPartsTest.cpp
)

set(SWIFT_LIBS
  lldbPluginSwiftLanguage
  lldbPluginSwiftLanguageRuntime
)

set(LLVM_OPTIONAL_SOURCES ${SWIFT_SOURCES})
if (NOT LLDB_ENABLE_SWIFT_SUPPORT)
  unset(SWIFT_SOURCES)
  unset(SWIFT_LIBS)
endif()

add_lldb_unittest(LLDBCoreTests
  DebuggerTest.cpp
  CommunicationTest.cpp
  DiagnosticEventTest.cpp
  DiagnosticsTest.cpp
  DumpDataExtractorTest.cpp
  DumpRegisterInfoTest.cpp
  FormatEntityTest.cpp
  MangledTest.cpp
  ModuleListTest.cpp
  ModuleSpecTest.cpp
  ModuleTest.cpp
  PluginManagerTest.cpp
  ProgressReportTest.cpp
  RichManglingContextTest.cpp
  SourceLocationSpecTest.cpp
  SourceManagerTest.cpp
  TelemetryTest.cpp
  UniqueCStringMapTest.cpp
  Value.cpp

  ${SWIFT_SOURCES}

  LINK_COMPONENTS
    Support
    Telemetry
  LINK_LIBS
    lldbCore
    lldbHost
    lldbPluginCPlusPlusLanguage
    lldbPluginDLanguage
    lldbPluginObjectFileELF
    lldbPluginObjectFileMachO
    lldbPluginObjectFilePECOFF
    lldbPluginPlatformMacOSX
    lldbPluginRustLanguage
    lldbPluginSymbolFileSymtab
    lldbSymbol
    lldbUtilityHelpers
    lldbHostHelpers
    LLVMTestingSupport
    ${SWIFT_LIBS}
  )
