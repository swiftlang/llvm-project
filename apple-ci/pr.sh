#!/bin/bash

# Build script for ci.swift.org PR testing.
# Tools like cmake/ninja needs to be in $PATH
# and run the script in build directory.

LLVM_PROJECT_SRC=$1

echo '--- CMake Config ---'
cmake -G Ninja \
 -DCMAKE_BUILD_TYPE=Release \
 -DLLVM_ENABLE_ASSERTIONS=On \
 '-DLLVM_ENABLE_PROJECTS=clang;lld' \
 '-DLLVM_TARGETS_TO_BUILD=X86;ARM;AArch64' \
 '-DLLVM_LIT_ARGS=-v' \
 ${LLVM_PROJECT_SRC}/llvm

echo '--- Ninja Build ---'
ninja -v
echo '--- Ninja Test ---'
ninja -v -k 0 check-all
