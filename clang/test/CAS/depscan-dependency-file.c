// RUN: rm -rf %t && mkdir -p %t
// RUN: split-file %s %t

// Baseline for comparison.
// RUN: %clang_cc1 -triple x86_64-apple-macos11 %t/main.c -emit-obj -o %t/output.o -isystem %t/sys \
// RUN:   -MT deps -dependency-file %t/regular.d

// Including system headers.
// RUN: %clang_cc1 -triple x86_64-apple-macos11 %t/main.c -emit-obj -o %t/output.o -isystem %t/sys \
// RUN:   -MT deps -sys-header-deps -dependency-file %t/regular-sys.d

// RUN: %clang -cc1depscan -fdepscan=inline -fdepscan-include-tree -o %t/t.rsp -cc1-args \
// RUN:   -cc1 -triple x86_64-apple-macos11 %t/main.c -emit-obj -o %t/output.o -isystem %t/sys \
// RUN:     -MT deps -dependency-file %t/t.d
// RUN: diff -u %t/regular.d %t/t.d

// Including system headers.
// RUN: %clang -cc1depscan -fdepscan=inline -fdepscan-include-tree -o %t/t.rsp -cc1-args \
// RUN:   -cc1 -triple x86_64-apple-macos11 %t/main.c -emit-obj -o %t/output.o -isystem %t/sys \
// RUN:     -MT deps -sys-header-deps -dependency-file %t/t-sys.d
// RUN: diff -u %t/regular-sys.d %t/t-sys.d

//--- main.c
#include "my_header.h"
#include <sys.h>

//--- my_header.h

//--- sys/sys.h
