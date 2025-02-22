add_lldb_unittest(LLDBValueObjectTests
  DumpValueObjectOptionsTests.cpp
  DynamicValueObjectLocalBuffer.cpp

  LINK_LIBS
    lldbValueObject
    lldbPluginPlatformLinux
    lldbPluginScriptInterpreterNone

  LINK_COMPONENTS
    Support
  )
