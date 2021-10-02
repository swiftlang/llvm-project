set(LLVM_LINK_COMPONENTS
  CAS
  CASObjectFormat
  JITLink
  Support
  )

add_llvm_unittest(CASObjectFormatTests
  EncodingTest.cpp
  FixupListTest.cpp
  ObjectFileSchemaTest.cpp
  TargetInfoListTest.cpp
  TargetListTest.cpp
  )

target_link_libraries(CASObjectFormatTests PRIVATE LLVMTestingSupport)
