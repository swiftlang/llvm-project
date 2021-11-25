// Test running -fdepscan.

// RUN: %clang -target x86_64-apple-macos11 -I %S/Inputs -fdepscan=daemon -Xclang -fcas -Xclang builtin -Xclang -fcas-builtin-path -Xclang %t/cas -fsyntax-only -x c %s
// RUN: %clang -target x86_64-apple-macos11 -I %S/Inputs -fdepscan=inline -Xclang -fcas -Xclang builtin -Xclang -fcas-builtin-path -Xclang %t/cas -fsyntax-only -x c %s
// RUN: %clang -target x86_64-apple-macos11 -I %S/Inputs -fdepscan=auto -Xclang -fcas -Xclang builtin -Xclang -fcas-builtin-path -Xclang %t/cas -fsyntax-only -x c %s
// RUN: %clang -target x86_64-apple-macos11 -I %S/Inputs -fdepscan=off -Xclang -fcas -Xclang builtin -Xclang -fcas-builtin-path -Xclang %t/cas -fsyntax-only -x c %s
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

#include "test.h"

int func(void);
