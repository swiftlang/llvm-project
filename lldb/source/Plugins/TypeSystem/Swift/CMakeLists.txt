# This dependency is part of a loop (TypeSystem<->ExpressionParser).
set_property(DIRECTORY PROPERTY LLDB_TOLERATED_PLUGIN_DEPENDENCIES
  SymbolFile
  ExpressionParser
)

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
    lldbPluginExpressionParserSwift

  LINK_COMPONENTS
    Support
)

# Enforce synchronization between Swift compiler updates and LLDB via PR testing.
check_cxx_compiler_flag("-Werror=switch" CXX_SUPPORTS_ERROR_SWITCH)
if (CXX_SUPPORTS_ERROR_SWITCH)
  target_compile_options(lldbPluginTypeSystemSwift PRIVATE -Werror=switch)
endif()
