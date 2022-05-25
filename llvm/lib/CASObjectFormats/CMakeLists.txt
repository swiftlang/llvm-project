add_llvm_component_library(LLVMCASObjectFormats
  Data.cpp
  FlatV1.cpp
  LinkGraph.cpp
  NestedV1.cpp
  ObjectFormatSchemaBase.cpp
  ObjectFormatHelpers.cpp
  Utils.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/CASObjectFormats

  DEPENDS
  intrinsics_gen

  LINK_COMPONENTS
  Support
  CAS
  JITLink
)
