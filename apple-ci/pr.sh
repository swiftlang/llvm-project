#!/bin/bash

# Build script for ci.swift.org PR testing.
# Tools like cmake/ninja needs to be in $PATH
# and run the script in build directory.

LLVM_PROJECT_SRC=$1
LLVM_ENABLE_PROJECTS=${2:-"clang;clang-tools-extra"}

# These flags are here so we can run the -fbounds-safety runtime tests inside
# compiler-rt.
# FIXME: We should be testing all of compiler-rt! Historically none of
# compiler-rt was tested by this script so the `-DCOMPILER_RT_BUILD_<name>=Off`
# lines are here to avoid building and testing as much as possible to minimize
# the problems that suddenly testing all of compiler-rt could cause. We should
# gradually start building and testing more of these runtimes (rdar://176397238).
BOUNDS_SAFETY_CMAKE_FLAGS=( \
  -DLLVM_ENABLE_RUNTIMES=compiler-rt \
  -DCOMPILER_RT_ENABLE_TEST_SUITES=bounds_safety \
  -DCOMPILER_RT_BOUNDS_SAFETY_USE_LLDB=On \
  -DCOMPILER_RT_BUILD_LIBFUZZER=Off \
  -DCOMPILER_RT_BUILD_MEMPROF=Off \
  -DCOMPILER_RT_BUILD_PROFILE=Off \
  -DCOMPILER_RT_BUILD_ORC=Off \
  -DCOMPILER_RT_BUILD_SANITIZERS=Off \
  -DCOMPILER_RT_BUILD_XRAY=Off \
)

echo '--- CMake Config ---'
cmake -G Ninja \
 -DCMAKE_BUILD_TYPE=Release \
 -DLLVM_ENABLE_ASSERTIONS=On \
 -DLLVM_ENABLE_PROJECTS=${LLVM_ENABLE_PROJECTS} \
 '-DLLVM_TARGETS_TO_BUILD=X86;ARM;AArch64' \
 '-DLLVM_LIT_ARGS=-v' \
 "${BOUNDS_SAFETY_CMAKE_FLAGS[@]}" \
 ${LLVM_PROJECT_SRC}/llvm

echo '--- Ninja Build ---'
ninja -v
echo '--- Ninja Test ---'
ninja -v -k 0 check-all
