if(NOT LLDB_BUILT_STANDALONE)
  set(tablegen_deps intrinsics_gen)
endif()

set_property(DIRECTORY PROPERTY LLDB_TOLERATED_PLUGIN_DEPENDENCIES 
  LanguageRuntime 
  TypeSystem
)

add_lldb_library(lldbPluginExpressionParserSwift PLUGIN
  SwiftASTManipulator.cpp
  SwiftExpressionParser.cpp
  SwiftExpressionSourceCode.cpp
  SwiftExpressionVariable.cpp
  SwiftPersistentExpressionState.cpp
  SwiftREPL.cpp
  SwiftREPLMaterializer.cpp
  SwiftUserExpression.cpp
  SwiftSILManipulator.cpp

  DEPENDS
  ${tablegen_deps}

  LINK_COMPONENTS
    Support
    Core
  LINK_LIBS
    lldbCore
    lldbDataFormatters
    lldbExpression
    lldbHost
    lldbInterpreter
    lldbSymbol
    lldbTarget
    lldbUtility
    lldbPluginTypeSystemSwift
    lldbPluginSwiftLanguageRuntime

  SWIFT_LIBS
    swiftAST
    swiftBasic
    swiftClangImporter
    swiftFrontend
    swiftIDE
    swiftParse
    swiftSIL
    swiftSILOptimizer
    swiftSerialization

  CLANG_LIBS
    clangAST
    clangBasic
    clangRewrite
  )
