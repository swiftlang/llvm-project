lldb_tablegen(LanguageSwiftProperties.inc -gen-lldb-property-defs
  SOURCE LanguageSwiftProperties.td
  TARGET LLDBPluginLanguageSwiftPropertiesGen)

lldb_tablegen(LanguageSwiftPropertiesEnum.inc -gen-lldb-property-enum-defs
  SOURCE LanguageSwiftProperties.td
  TARGET LLDBPluginLanguageSwiftPropertiesEnumGen)

set(LLVM_NO_RTTI 1)

add_lldb_library(lldbPluginSwiftLanguage PLUGIN
  FoundationValueTypes.cpp
  LogChannelSwift.cpp
  ObjCRuntimeSyntheticProvider.cpp
  SwiftArray.cpp
  SwiftBasicTypes.cpp
  SwiftDictionary.cpp
  SwiftFormatters.cpp
  SwiftHashedContainer.cpp
  SwiftLanguage.cpp
  SwiftMetatype.cpp
  SwiftOptionSet.cpp
  SwiftOptional.cpp
  SwiftFrameRecognizers.cpp
  SwiftSet.cpp
  SwiftUIFormatters.cpp
  SwiftUnsafeTypes.cpp
  SystemValueTypes.cpp

  LINK_COMPONENTS
    Support
  LINK_LIBS
    lldbCore
    lldbDataFormatters
    lldbSymbol
    lldbTarget
    lldbUtility
    lldbPluginObjCLanguage
    lldbPluginTypeSystemSwift

  SWIFT_LIBS
    swiftAST
    swiftClangImporter

  CLANG_LIBS
    clangAST
)
if(CMAKE_CXX_COMPILER_ID STREQUAL Clang AND NOT SWIFT_COMPILER_MSVC_LIKE)
  target_compile_options(lldbPluginSwiftLanguage PRIVATE
    -Wno-dollar-in-identifier-extension)
endif()

add_dependencies(lldbPluginSwiftLanguage
  LLDBPluginLanguageSwiftPropertiesGen
  LLDBPluginLanguageSwiftPropertiesEnumGen)

# SwiftLanguage::Initialize() calls initializeSwiftModules().
if (NOT LLDB_LINK_SWIFT_COMPILER_DYLIB)
  if(BOOTSTRAPPING_MODE OR
     (NOT LLDB_BUILT_STANDALONE AND NOT DEFINED BOOTSTRAPPING_MODE))
    target_link_libraries(lldbPluginSwiftLanguage
      PRIVATE
        swiftCompilerModules)

    # The swiftCompilerModules need swiftrt to work properly.
    string(REGEX MATCH "^[^-]*" arch ${LLVM_TARGET_TRIPLE})
    if (CMAKE_SYSTEM_NAME MATCHES "Linux|Android|OpenBSD|FreeBSD")
      string(TOLOWER ${CMAKE_SYSTEM_NAME} platform)
      target_link_libraries(lldbPluginSwiftLanguage
        PRIVATE
          "${LLDB_SWIFT_LIBS}/${platform}/${arch}/swiftrt${CMAKE_C_OUTPUT_EXTENSION}")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
      set(swiftrt_obj
          ${SWIFT_PATH_TO_SWIFT_SDK}/usr/lib/swift/windows/${arch}/swiftrt${CMAKE_C_OUTPUT_EXTENSION})
      target_link_libraries(lldbPluginSwiftLanguage PRIVATE ${swiftrt_obj})
    endif()
  else()
    target_link_libraries(lldbPluginSwiftLanguage
      PRIVATE
        swiftCompilerStub)
  endif()
endif()
