// TODO: Test should be updated to not depend on a working cache at default location, which is out of test/build directory.

// Check with prefix mapping:
//
// RUN: rm -rf %t.d
// RUN: mkdir %t.d
// RUN: %clang -cc1depscan -dump-depscan-tree=%t.root -fdepscan=inline    \
// RUN:    -fdepscan-prefix-map=%S=/^source                               \
// RUN:    -fdepscan-prefix-map=%t.d=/^testdir                            \
// RUN:    -fdepscan-prefix-map=%{objroot}=/^objroot                      \
// RUN:    -fdepscan-prefix-map-toolchain=/^toolchain                     \
// RUN:    -fdepscan-prefix-map-sdk=/^sdk                                 \
// RUN:    -cc1-args -triple x86_64-apple-macos11.0 -x c %s -o %t.d/out.o \
// RUN:              -isysroot %S/Inputs/SDK                              \
// RUN:              -working-directory %t.d                              \
// RUN: | FileCheck %s
//
// CHECK:      "-fcas-path" "auto"
// CHECK-SAME: "-working-directory" "/^testdir"
// CHECK-SAME: "-x" "c" "/^source/depscan-prefix-map.c"
// CHECK-SAME: "-isysroot" "/^sdk"

// RUN: llvm-cas --cas auto --ls-tree-recursive @%t.root                  \
// RUN: | FileCheck %s -check-prefix=CHECK-ROOT
//
// RUN: llvm-cas --cas auto --ls-tree-recursive @%t.root                  \
// RUN: | FileCheck %s -check-prefix=CHECK-ROOT
//
// CHECK-ROOT:      tree
// CHECK-ROOT-SAME:             /^objroot/test/CAS/{{$}}
// CHECK-ROOT-NEXT: tree {{.*}} /^sdk/Library/Frameworks/{{$}}
// CHECK-ROOT-NEXT: file {{.*}} /^source/depscan-prefix-map.c{{$}}

int test() { return 0; }
