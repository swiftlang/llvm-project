add_lldb_unittest(LanguageSwiftLanguageTests
  SwiftLanguageTest.cpp

  LINK_LIBS
    lldbPluginSwiftLanguage
    lldbPluginSwiftLanguageRuntime
)
