add_llvm_component_library(LLVMCASObjectFormats
  Data.cpp
  FlatV1.cpp
  NestedV1.cpp
  SchemaBase.cpp
  ObjectFormatHelpers.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/CASObjectFormats

  LINK_COMPONENTS
  Support
  CAS
  JITLink
)
