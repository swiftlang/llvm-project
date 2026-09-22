if(NOT LLVM_ENABLE_ONDISK_CAS)
  message(STATUS "Not building llvm-cas-bench: LLVM_ENABLE_ONDISK_CAS is off")
  return()
endif()

set(LLVM_LINK_COMPONENTS
  Support
  CAS
  )

add_llvm_tool(llvm-cas-bench
  CASBench.cpp
  KeyValueCAS.cpp
  LLVMCASBackend.cpp
  FileCAS.cpp
  SQLiteCAS.cpp
  LMDBCAS.cpp
  )

# The key/value backends derive from cas::builtin::BuiltinCAS so that BLAKE3
# hashing, CASID parsing and the error vocabulary are the same code LLVMCAS
# uses rather than reimplementations. That header is internal to the CAS
# library, so reach it directly.
target_include_directories(llvm-cas-bench PRIVATE ${LLVM_MAIN_SRC_DIR}/lib/CAS)

# SQLite: FindSQLite3 is a CMake builtin and resolves to the platform SQLite.
find_package(SQLite3)
if(SQLite3_FOUND)
  target_link_libraries(llvm-cas-bench PRIVATE SQLite3::SQLite3)
  target_compile_definitions(llvm-cas-bench PRIVATE CASBENCH_HAVE_SQLITE=1)
  message(STATUS "llvm-cas-bench: SQLite ${SQLite3_VERSION}")
else()
  target_compile_definitions(llvm-cas-bench PRIVATE CASBENCH_HAVE_SQLITE=0)
  message(STATUS "llvm-cas-bench: SQLite not found, --backend=sqlite disabled")
endif()

# LMDB has no CMake find module anywhere, so probe for it directly.
find_path(LMDB_INCLUDE_DIR lmdb.h
  HINTS ${LMDB_ROOT}/include /opt/homebrew/include /usr/local/include)
find_library(LMDB_LIBRARY NAMES lmdb
  HINTS ${LMDB_ROOT}/lib /opt/homebrew/lib /usr/local/lib)
if(LMDB_INCLUDE_DIR AND LMDB_LIBRARY)
  target_include_directories(llvm-cas-bench PRIVATE ${LMDB_INCLUDE_DIR})
  target_link_libraries(llvm-cas-bench PRIVATE ${LMDB_LIBRARY})
  target_compile_definitions(llvm-cas-bench PRIVATE CASBENCH_HAVE_LMDB=1)
  message(STATUS "llvm-cas-bench: LMDB ${LMDB_LIBRARY}")
else()
  target_compile_definitions(llvm-cas-bench PRIVATE CASBENCH_HAVE_LMDB=0)
  message(STATUS
    "llvm-cas-bench: LMDB not found, --backend=lmdb disabled "
    "(install it, or pass -DLMDB_ROOT=<prefix>)")
endif()
