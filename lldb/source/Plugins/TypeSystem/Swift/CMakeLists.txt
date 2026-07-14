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
