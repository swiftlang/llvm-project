add_lldb_library(lldbPluginTypeSystemSwift PLUGIN
  SwiftDWARFImporterForClangTypes.cpp
  TypeSystemSwift.cpp
  TypeSystemSwiftTypeRef.cpp
  SwiftASTContext.cpp
  SwiftDemangle.cpp

  LINK_LIBS
    lldbCore
    lldbSymbol
    lldbTarget
    lldbUtility

  LINK_COMPONENTS
    Support
)
