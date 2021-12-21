add_llvm_component_library(LLVMCAS
  ActionCache.cpp
  ActionDescription.cpp
  BuiltinCAS.cpp
  CASDB.cpp
  CASFileSystem.cpp
  CASOutputBackend.cpp
  CachingOnDiskFileSystem.cpp
  FileSystemCache.cpp
  HashMappedTrie.cpp
  HierarchicalTreeBuilder.cpp
  InMemoryActionCache.cpp
  Namespace.cpp
  NamespaceHelpers.cpp
  OnDiskActionCache.cpp
  OnDiskHashMappedTrie.cpp
  PluginCAS.cpp
  UniqueID.cpp
  Utils.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/CAS

  LINK_COMPONENTS
  BinaryFormat
)
