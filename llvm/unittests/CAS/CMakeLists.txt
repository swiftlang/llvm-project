set(LLVM_LINK_COMPONENTS
  Support
  CAS
  TestingSupport
  )

add_llvm_unittest(CASTests
  CASDBTest.cpp
  CASFileSystemTest.cpp
  CASOutputBackendTest.cpp
  CASUtilsTest.cpp
  CachingOnDiskFileSystemTest.cpp
  FlatUniqueIDArrayRefTest.cpp
  HashMappedTrieTest.cpp
  HierarchicalTreeBuilderTest.cpp
  NamespaceTest.cpp
  TestingNamespaces.cpp
  UniqueIDTest.cpp
  )

target_link_libraries(CASTests PRIVATE LLVMTestingSupport)
