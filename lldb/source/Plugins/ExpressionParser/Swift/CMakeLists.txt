if(NOT LLDB_BUILT_STANDALONE)
  set(tablegen_deps intrinsics_gen)
endif()

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
    swiftAST
    swiftBasic
    swiftClangImporter
    swiftFrontend
    swiftIDE
    swiftParse
    swiftSIL
    swiftSILOptimizer
    swiftSerialization
    clangAST
    clangBasic
    clangRewrite

  LINK_COMPONENTS
    Support
    Core
  )

if(BOOTSTRAPPING_MODE)
  target_link_libraries(lldbPluginExpressionParserSwift
    PRIVATE
      swiftCompilerModules)
  if(LLDB_SWIFT_LLVM_BINDINGS_LIBS)
    # Make sure the linker can discover Swift LLVM Bindings artifacts.
    # See https://github.com/apple/swift-llvm-bindings.
    target_link_directories(lldbPluginExpressionParserSwift PUBLIC
        ${LLDB_SWIFT_LLVM_BINDINGS_LIBS})
  endif()
else()
  target_link_libraries(lldbPluginExpressionParserSwift
    PRIVATE
      swiftCompilerStub)
endif()
