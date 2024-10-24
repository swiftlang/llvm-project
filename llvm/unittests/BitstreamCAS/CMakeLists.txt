set(LLVM_LINK_COMPONENTS
  CAS
  )

add_llvm_unittest(BitstreamCASTests
  BitstreamCASWriterTest.cpp
  )
