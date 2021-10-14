// TODO: Test should be updated to not depend on a working cache at default location, which is out of test/build directory.

// RUN: %clang -cc1depscan -cc1-args -cc1 -triple x86_64-apple-macos11.0 -x c %s -o %s.o 2>&1 | FileCheck %s
// RUN: %clang -cc1depscan -cc1-args -triple x86_64-apple-macos11.0 -x c %s -o %s.o 2>&1 | FileCheck %s

// CHECK: "-fcas" "builtin"

int test() { return 0; }
