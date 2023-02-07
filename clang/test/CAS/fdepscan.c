// Test running -fdepscan.
//
// TODO: Test should be updated to not depend on a working cache at default
// location, which is out of test/build directory.

// RUN: %clang -target x86_64-apple-macos11 -I %S/Inputs -fdepscan=daemon -fsyntax-only -x c %s
// RUN: %clang -target x86_64-apple-macos11 -I %S/Inputs -fdepscan=inline -fsyntax-only -x c %s
// RUN: %clang -target x86_64-apple-macos11 -I %S/Inputs -fdepscan=auto -fsyntax-only -x c %s
// RUN: %clang -target x86_64-apple-macos11 -I %S/Inputs -fdepscan=off -fsyntax-only -x c %s
//
// Check -fdepscan-share-related arguments are claimed.
// TODO: Check behaviour.
//
// RUN: %clang -target x86_64-apple-macos11 -I %S/Inputs -fdepscan=off \
// RUN:     -fdepscan-share-parent                                     \
// RUN:     -fdepscan-share-parent=                                    \
// RUN:     -fdepscan-share-parent=python                              \
// RUN:     -fdepscan-share=python                                     \
// RUN:     -fdepscan-share=                                           \
// RUN:     -fdepscan-share-stop=python                                \
// RUN:     -fno-depscan-share                                         \
// RUN:     -fsyntax-only -x c %s                                      \
// RUN: | FileCheck %s -allow-empty
// CHECK-NOT: warning:
//
// RUN: not %clang -target x86_64-apple-macos11 -I %S/Inputs \
// RUN:     -fdepscan-share-parents 2>&1                     \
// RUN: | FileCheck %s -check-prefix=BAD-SPELLING
// BAD-SPELLING: error: unknown argument '-fdepscan-share-parents'

#include "test.h"

int func(void);
