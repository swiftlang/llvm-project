if (LLVM_ENABLE_ONDISK_CAS)
  add_definitions(-DLLVM_ENABLE_ONDISK_CAS=1)
endif()

add_llvm_component_library(LLVMCAS
  ActionCache.cpp
  ActionCaches.cpp
  BuiltinCAS.cpp
  BuiltinUnifiedCASDatabases.cpp
  CASConfiguration.cpp
  CASFileSystem.cpp
  CASNodeSchema.cpp
  CASOutputBackend.cpp
  CASProvidingFileSystem.cpp
  CachingOnDiskFileSystem.cpp
  DatabaseFile.cpp
  FileSystemCache.cpp
  HierarchicalTreeBuilder.cpp
  InMemoryCAS.cpp
  MappedFileRegionArena.cpp
  ObjectStore.cpp
  OnDiskCAS.cpp
  OnDiskCASLogger.cpp
  OnDiskCommon.cpp
  OnDiskDataAllocator.cpp
  OnDiskGraphDB.cpp
  OnDiskTrieRawHashMap.cpp
  OnDiskKeyValueDB.cpp
  PluginCAS.cpp
  TreeSchema.cpp
  UnifiedOnDiskCache.cpp
  Utils.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/CAS

  LINK_LIBS
  ${LLVM_PTHREAD_LIB}

  LINK_COMPONENTS
  Support
)
