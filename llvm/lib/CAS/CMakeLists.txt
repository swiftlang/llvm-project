add_llvm_component_library(LLVMCAS
  BuiltinCAS.cpp
  CASDB.cpp
  CASFileSystem.cpp
  FileSystemCache.cpp
  HashMappedTrie.cpp
  OnDiskHashMappedTrie.cpp
  PluginCAS.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/CAS

  LINK_COMPONENTS
  Support
)
