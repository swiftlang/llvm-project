set_property(DIRECTORY PROPERTY LLDB_TOLERATED_PLUGIN_DEPENDENCIES Language ExpressionParser)

add_lldb_library(lldbPluginSwiftLanguageRuntime PLUGIN
  LLDBMemoryReader.cpp
  ReflectionContext.cpp
  SwiftLanguageRuntime.cpp
  SwiftLanguageRuntimeDynamicTypeResolution.cpp
  SwiftLanguageRuntimeNames.cpp
  SwiftLanguageRuntimeRemoteAST.cpp
  SwiftMetadataCache.cpp
  SwiftTask.cpp

  LINK_LIBS
    swiftAST
    swiftBasic
    swiftRemoteInspection
    swiftRemoteAST
    lldbCore
    lldbExpression
    lldbInterpreter
    lldbTarget
    lldbUtility
    lldbPluginExpressionParserClang
    # SwiftLanguageRuntime.cpp calls
    # SwiftPersistentExpressionState::RegisterSwiftPersistentDecl, defined in the
    # ExpressionParser/Swift plugin. Declaring the edge here (alongside the
    # reverse edge in ExpressionParser/Swift) lets CMake resolve the static-lib
    # cycle so the single-pass gold linker (-z defs) links liblldb on Linux.
    lldbPluginExpressionParserSwift
    lldbPluginProcessUtility
    lldbPluginSwiftLanguage
)
