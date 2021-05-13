set(LLVM_LINK_COMPONENTS
  Support
  CAS
  )

add_llvm_unittest(CASTests
  CASDBTest.cpp
  CASFileSystemTest.cpp
  CachingOnDiskFileSystemTest.cpp
  HashMappedTrieTest.cpp
  HierarchicalTreeBuilderTest.cpp
  )
