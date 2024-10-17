set(LLVM_NO_RTTI 1)

add_subdirectory(SwiftHighlighter)

add_lldb_library(lldbPluginSwiftLanguage PLUGIN
  FoundationValueTypes.cpp
  LogChannelSwift.cpp
  ObjCRuntimeSyntheticProvider.cpp
  SwiftArray.cpp
  SwiftBasicTypes.cpp
  SwiftDictionary.cpp
  SwiftFormatters.cpp
  SwiftHashedContainer.cpp
  SwiftHighlighterBridge.cpp
  SwiftLanguage.cpp
  SwiftMetatype.cpp
  SwiftOptionSet.cpp
  SwiftOptional.cpp
  SwiftRuntimeFailureRecognizer.cpp
  SwiftSet.cpp
  SwiftUnsafeTypes.cpp

  LINK_LIBS
    lldbCore
    lldbDataFormatters
    lldbSymbol
    lldbTarget
    lldbUtility
    lldbPluginObjCLanguage
    lldbPluginTypeSystemSwift
    swiftAST
    swiftClangImporter
    clangAST
    swiftHighlighter

  LINK_COMPONENTS
    Support
)
if(CMAKE_CXX_COMPILER_ID STREQUAL Clang AND NOT SWIFT_COMPILER_MSVC_LIKE)
  target_compile_options(lldbPluginSwiftLanguage PRIVATE
    -Wno-dollar-in-identifier-extension)
endif()

