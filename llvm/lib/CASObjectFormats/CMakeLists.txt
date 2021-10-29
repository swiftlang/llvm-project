add_llvm_component_library(LLVMCASObjectFormats
  Data.cpp
  NestedV1.cpp
  SchemaBase.cpp
  SimpleV1.cpp
  ObjectFormatHelpers.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/CASObjectFormats

  LINK_COMPONENTS
  Support
  CAS
  JITLink
)
