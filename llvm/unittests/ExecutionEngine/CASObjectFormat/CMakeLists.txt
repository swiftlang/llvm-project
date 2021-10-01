set(LLVM_LINK_COMPONENTS
  CAS
  CASObjectFormat
  JITLink
  Support
  )

add_llvm_unittest(CASObjectFormatTests
  FixupListTest.cpp
  ObjectFileSchemaTest.cpp
  TargetInfoListTest.cpp
  )

target_link_libraries(CASObjectFormatTests PRIVATE LLVMTestingSupport)
