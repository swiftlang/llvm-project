// Test running -fcas-token-cache.
//
// TODO: Remove the dependency on driver (-fdepscan should get coverage somewhere else instead).

// RUN: %clang -target x86_64-apple-macos11 -I %S/Inputs -fdepscan=inline -Xclang -fcas-token-cache -Xclang -fcas -Xclang builtin -Xclang -fcas-builtin-path -Xclang %t/cas -fsyntax-only -x c %s

#include "test.h"

int func(void);
