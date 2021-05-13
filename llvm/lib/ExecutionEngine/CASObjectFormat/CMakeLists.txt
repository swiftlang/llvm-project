add_llvm_component_library(LLVMCASObjectFormat
  ObjectFileSchema.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/ExecutionEngine/CASObjectFormat

  LINK_COMPONENTS
  Support
  CAS
  JITLink
)
