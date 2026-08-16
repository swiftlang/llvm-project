# The ObjC language plugin has no code references to the Swift language plugin
# (no swift:: / SwiftLanguage usage in its sources). The former
# `SWIFT_LIBS = lldbPluginSwiftLanguage` link dep was vestigial, but because core
# lldbValueObject links lldbPluginObjCLanguage, it dragged lldbPluginSwiftLanguage
# -> swiftAST -> swiftClangImporter -> the Clang frontend into every tool,
# including lldb-server (which registers none of those plugins). liblldb still
# links lldbPluginSwiftLanguage via LLDB_ALL_PLUGINS.

add_lldb_library(lldbPluginObjCLanguage PLUGIN
  ObjCLanguage.cpp
  CF.cpp
  CFBasicHash.cpp
  Cocoa.cpp
  CoreMedia.cpp
  NSArray.cpp
  NSDictionary.cpp
  NSError.cpp
  NSException.cpp
  NSIndexPath.cpp
  NSSet.cpp
  NSString.cpp

  LINK_LIBS
    lldbCore
    lldbDataFormatters
    lldbExpression
    lldbHost
    lldbSymbol
    lldbTarget
    lldbUtility
    lldbValueObject
    lldbPluginAppleObjCRuntime
    lldbPluginClangCommon
    lldbPluginTypeSystemClang
  CLANG_LIBS
    clangAST
)

if (CXX_SUPPORTS_NO_GNU_ANONYMOUS_STRUCT)
  target_compile_options(lldbPluginObjCLanguage
    PRIVATE -Wno-gnu-anonymous-struct)
endif ()

if (CXX_SUPPORTS_NO_NESTED_ANON_TYPES)
  target_compile_options(lldbPluginObjCLanguage
    PRIVATE -Wno-nested-anon-types)
endif ()
