// Verify `-fmodules-force-redundant-lookup` is recognized in a typical
// compilation that relies on build session optimizations.

// RUN: rm -rf %t
// RUN: split-file %s %t

// RUN: touch %t/session.timestamp

// RUN: %clang -fmodules -fimplicit-module-maps -fsyntax-only %t/tu1.c \
// RUN:   -fmodules-cache-path=%t/cache -I%t/include \
// RUN:   -fbuild-session-file=%t/session.timestamp -fmodules-validate-once-per-build-session \
// RUN:   -Xclang -fmodules-force-redundant-lookup 2>&1 | FileCheck %s --allow-empty

// CHECK-NOT: warning: 
// CHECK-NOT: error: 

//--- include/module.modulemap
module Dep { header "Dep.h" }
//--- include/Dep.h
int foo(void);

//--- tu1.c
#include "Dep.h"
