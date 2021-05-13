set(LLVM_LINK_COMPONENTS
  Support
  JITLink
  CAS
  CASObjectFormat
  )

add_llvm_tool(llvm-cas-object-format
  llvm-cas-object-format.cpp
  )
