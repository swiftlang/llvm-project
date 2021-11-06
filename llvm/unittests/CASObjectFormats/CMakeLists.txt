set(LLVM_LINK_COMPONENTS
  CAS
  CASObjectFormats
  JITLink
  Support
  )

add_llvm_unittest(CASObjectFormatsTests
  EncodingTest.cpp
  BlockDataTest.cpp
  FixupListTest.cpp
  NestedV1SchemaTest.cpp
  NestedV1TargetListTest.cpp
  TargetInfoListTest.cpp
  )

target_link_libraries(CASObjectFormatsTests PRIVATE LLVMTestingSupport)
