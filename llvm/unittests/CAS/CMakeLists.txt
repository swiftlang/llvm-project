set(LLVM_LINK_COMPONENTS
  Support
  CAS
  )

add_llvm_unittest(CASTests
  CASDBTest.cpp
  CASFileSystemTest.cpp
  HashMappedTrieTest.cpp
  )
