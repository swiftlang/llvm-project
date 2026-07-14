# TypeSystemSwiftTypeRef.cpp constructs SwiftPersistentExpressionState and
# SwiftUserExpression, which live in the ExpressionParser/Swift plugin.
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
    # SwiftPersistentExpressionState / SwiftUserExpression are constructed by
    # TypeSystemSwiftTypeRef.cpp. This plugin and lldbPluginExpressionParserSwift
    # reference each other's symbols; declaring the edge here (in addition to the
    # reverse edge in ExpressionParser/Swift) lets CMake resolve the static-lib
    # cycle so the single-pass gold linker (-z defs) links liblldb on Linux.
    lldbPluginExpressionParserSwift

    ## BEGIN SWIFT
    # SwiftASTContext.cpp hosts the embedded Swift compiler + ClangImporter, so
    # this plugin (not core lldbSymbol) owns the Swift/Clang frontend deps. Only
    # full-lldb consumers that link this plugin (liblldb) pull them in; tools
    # like lldb-server that don't link it stay free of the frontend.
    clangAST
    clangBasic
    clangFrontend
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
)

# Enforce synchronization between Swift compiler updates and LLDB via PR testing.
check_cxx_compiler_flag("-Werror=switch" CXX_SUPPORTS_ERROR_SWITCH)
if (CXX_SUPPORTS_ERROR_SWITCH)
  target_compile_options(lldbPluginTypeSystemSwift PRIVATE -Werror=switch)
endif()
