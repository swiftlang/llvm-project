# The ExpressionParser dependency is part of a loop
# (LanguageRuntime<->ExpressionParser).
set_property(DIRECTORY PROPERTY LLDB_TOLERATED_PLUGIN_DEPENDENCIES
  Language
  ExpressionParser
)

add_lldb_library(lldbPluginSwiftLanguageRuntime PLUGIN
  LLDBMemoryReader.cpp
  ReflectionContext.cpp
  SwiftLanguageRuntime.cpp
  SwiftLanguageRuntimeDynamicTypeResolution.cpp
  SwiftLanguageRuntimeNames.cpp
  SwiftLanguageRuntimeRemoteAST.cpp
  SwiftMetadataCache.cpp
  SwiftTask.cpp

  SWIFT_LIBS
    swiftAST
    swiftBasic
    swiftRemoteInspection
    swiftRemoteAST

  LINK_LIBS
    lldbCore
    lldbExpression
    lldbInterpreter
    lldbTarget
    lldbUtility
    lldbPluginExpressionParserClang
    lldbPluginExpressionParserSwift
    lldbPluginProcessUtility
    lldbPluginSwiftLanguage
)
