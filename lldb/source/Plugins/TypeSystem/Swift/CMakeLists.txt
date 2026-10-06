set_property(DIRECTORY PROPERTY LLDB_TOLERATED_PLUGIN_DEPENDENCIES ExpressionParser)

add_lldb_library(lldbPluginTypeSystemSwift PLUGIN
  LLDBExplicitModuleLoader.cpp
  LLDBImplicitModuleLoader.cpp
  SwiftDWARFImporterForClangTypes.cpp
  TypeSystemSwift.cpp
  TypeSystemSwiftTypeRef.cpp
  SwiftASTContext.cpp

  LINK_LIBS
    lldbCore
    lldbSymbol
    lldbTarget
    lldbUtility
    # Cyclic with lldbPluginExpressionParserSwift; needed by single-pass linkers.
    lldbPluginExpressionParserSwift

    ## BEGIN SWIFT
    swiftAST
    swiftASTSectionImporter
    swiftBasic
    swiftClangImporter
    swiftFrontend
    swiftSIL
    swiftSerialization
    ## END SWIFT

  LINK_COMPONENTS
    Support
  CLANG_LIBS
    clangAST
    clangBasic
    clangFrontend
)

# Enforce synchronization between Swift compiler updates and LLDB via PR testing.
check_cxx_compiler_flag("-Werror=switch" CXX_SUPPORTS_ERROR_SWITCH)
if (CXX_SUPPORTS_ERROR_SWITCH)
  target_compile_options(lldbPluginTypeSystemSwift PRIVATE -Werror=switch)
endif()
