// Test that a module's directory dependencies are reachable through the
// libclang dependency scanning C API.

// REQUIRES: shell

// RUN: rm -rf %t
// RUN: split-file %s %t

// RUN: c-index-test core -scan-deps -working-dir %t -- %clang -fmodules \
// RUN:   -fmodules-cache-path=%t/cache -I %t/include -c %t/tu.c -o %t/tu.o \
// RUN:   | sed 's:\\\\\?:/:g' | FileCheck %s -DPREFIX=%/t

// CHECK:      name: UmbDir
// CHECK:      file-deps:
// CHECK-NEXT:   [[PREFIX]]/include/Umb/module.modulemap
// CHECK-NEXT:   [[PREFIX]]/include/Umb/sub/a.h
// CHECK-NEXT: directory-deps:
// CHECK-NEXT:   [[PREFIX]]/include/Umb/sub

//--- include/Umb/module.modulemap
module UmbDir {
  umbrella "sub"
  module * { export * }
}

//--- include/Umb/sub/a.h

//--- tu.c
#include "Umb/sub/a.h"
