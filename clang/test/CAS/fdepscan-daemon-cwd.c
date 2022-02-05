// Test running -fdepscan with a daemon launched from different directory.
//
// REQUIRES: system-darwin

// RUN: rm -rf %t && mkdir -p %t/include
// RUN: cp %S/Inputs/test.h %t/include
// RUN: %clang -cc1depscand -start %t/depscand
// RUN: (cd %t && %clang -target x86_64-apple-macos11 -fdepscan=daemon    \
// RUN:    -fdepscan-prefix-map=%S=/^source                               \
// RUN:    -fdepscan-prefix-map=%t=/^build                                \
// RUN:    -fdepscan-prefix-map-toolchain=/^toolchain                     \
// RUN:    -fdepscan-daemon=%t/depscand -Iinclude -fsyntax-only -x c %s)
// RUN: %clang -cc1depscand -shutdown %t/depscand

#include "test.h"

int func(void);
