
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -verify %s
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety  -fdiagnostics-parseable-fixits %s 2>&1 | FileCheck %s --allow-empty
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -x objective-c -fexperimental-bounds-safety-objc -verify %s
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety  -x objective-c -fexperimental-bounds-safety-objc -fdiagnostics-parseable-fixits %s 2>&1 | FileCheck %s --allow-empty
// expected-no-diagnostics

#include <ptrcheck.h>

int len;

// void *__counted_by is allowed (treated as byte count)
void *__attribute__((__counted_by__(len))) voidPtr;

// void *__counted_by is allowed (treated as byte count)
#define my_custom_counted_by(X) __attribute__((__counted_by__(X)))
void * my_custom_counted_by(len) voidPtr2;

// CHECK-NOT: fix-it:"{{.+}}":
