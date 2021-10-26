set(LLVM_LINK_COMPONENTS
  Support
  JITLink
  CAS
  CASObjectFormats
  )

add_llvm_tool(llvm-cas-object-format
  llvm-cas-object-format.cpp
  )
