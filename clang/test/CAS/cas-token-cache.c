// Test running -fcas-token-cache.
// TODO: Test should be updated to not depend on a working cache at default location, which is out of test/build directory.
// Removing the dependency on driver will be great as well.

// RUN: %clang -target x86_64-apple-macos11 -I %S/Inputs -fdepscan=daemon -Xclang -fcas-token-cache -fsyntax-only -x c %s

#include "test.h"

int func(void);
