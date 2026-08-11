// Test that a module's directory dependencies are reachable through the
// libclang dependency scanning C API, and that reporting one as changed
// rebuilds the scanning module that depends on it.

// RUN: rm -rf %t
// RUN: split-file %s %t

// DEFINE: %{scan} = c-index-test core -scan-deps -working-dir %t
// DEFINE: %{cc} = -- %clang -fmodules -fmodules-cache-path=%t/cache \
// DEFINE:   -I %t/include -c %t/tu.c -o %t/tu.o

// RUN: %{scan} %{cc} | sed 's:\\\\\?:/:g' | FileCheck %s -DPREFIX=%/t

// CHECK:      name: UmbDir
// CHECK:      file-deps:
// CHECK-NEXT:   [[PREFIX]]/include/Umb/module.modulemap
// CHECK-NEXT:   [[PREFIX]]/include/Umb/sub/a.h
// CHECK-NEXT: directory-deps:
// CHECK-NEXT:   [[PREFIX]]/include/Umb/sub

// Without reporting the change, the cached scanning module is reused. Module
// files written in the second a scan starts count as up to date with it, so
// leave a second between builds.
// RUN: sleep 1
// RUN: touch %t/include/Umb/sub/b.h
// RUN: %{scan} %{cc} | sed 's:\\\\\?:/:g' \
// RUN:   | FileCheck %s -DPREFIX=%/t --check-prefix=STALE

// STALE:     name: UmbDir
// STALE-NOT: [[PREFIX]]/include/Umb/sub/b.h
// STALE:     directory-deps:

// RUN: %{scan} -invalidated-path %t/include/Umb/sub %{cc} \
// RUN:   | sed 's:\\\\\?:/:g' \
// RUN:   | FileCheck %s -DPREFIX=%/t --check-prefix=FRESH-B

// FRESH-B:      name: UmbDir
// FRESH-B:      file-deps:
// FRESH-B-NEXT:   [[PREFIX]]/include/Umb/module.modulemap
// FRESH-B-NEXT:   [[PREFIX]]/include/Umb/sub/a.h
// FRESH-B-NEXT:   [[PREFIX]]/include/Umb/sub/b.h

// The deprecated entry point behaves the same.
// RUN: sleep 1
// RUN: touch %t/include/Umb/sub/c.h
// RUN: %{scan} -invalidated-directory %t/include/Umb/sub %{cc} \
// RUN:   | sed 's:\\\\\?:/:g' \
// RUN:   | FileCheck %s -DPREFIX=%/t --check-prefix=FRESH-C

// FRESH-C:      name: UmbDir
// FRESH-C:      file-deps:
// FRESH-C-NEXT:   [[PREFIX]]/include/Umb/module.modulemap
// FRESH-C-NEXT:   [[PREFIX]]/include/Umb/sub/a.h
// FRESH-C-NEXT:   [[PREFIX]]/include/Umb/sub/b.h
// FRESH-C-NEXT:   [[PREFIX]]/include/Umb/sub/c.h

//--- include/Umb/module.modulemap
module UmbDir {
  umbrella "sub"
  module * { export * }
}

//--- include/Umb/sub/a.h

//--- tu.c
#include "Umb/sub/a.h"
