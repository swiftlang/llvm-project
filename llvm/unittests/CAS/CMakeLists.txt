set(LLVM_LINK_COMPONENTS
  Support
  CAS
  TestingSupport
  )

add_llvm_unittest(CASTests
  CASDBTest.cpp
  CASFileSystemTest.cpp
  CASOptionsTest.cpp
  CASOutputBackendTest.cpp
  CASUtilsTest.cpp
  CachingOnDiskFileSystemTest.cpp
  HashMappedTrieTest.cpp
  OnDiskHashMappedTrieTest.cpp
  HierarchicalTreeBuilderTest.cpp
  )

target_link_libraries(CASTests PRIVATE LLVMTestingSupport)
