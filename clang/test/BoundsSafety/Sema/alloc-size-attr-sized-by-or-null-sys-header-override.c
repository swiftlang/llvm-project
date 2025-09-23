// RUN: %clang_cc1 -fsyntax-only -Wno-strict-prototypes -fbounds-safety -verify %s
// RUN: not %clang_cc1 -fsyntax-only -Wno-strict-prototypes -fbounds-safety -fdiagnostics-parseable-fixits %s 2>&1 | FileCheck %s --implicit-check-not fix-it
// RUN: %clang_cc1 -fsyntax-only -Wno-strict-prototypes -fbounds-safety -x objective-c -fexperimental-bounds-safety-objc -verify %s

#include <ptrcheck.h>
#include "alloc-size-attr-sized-by-or-null-sys-header.h"


__attribute__((alloc_size(1))) void * unnamed_param(unsigned size) {
    return (void*)0;
}

void * __sized_by_or_null(size) inherit_attr(unsigned size) {
    return (void*)0;
}

void * __sized_by_or_null(size) unnamed_param_inherit_attr(unsigned size) {
    return (void*)0;
}

// expected-error@+2{{invalid return type 'void *__single __sized_by_or_null(count * size)' (aka 'void *__single') for function with alloc_size attribute; '__sized_by_or_null(size * count)' or '__sized_by(size * count)' required}}
// CHECK: fix-it:"{{.*}}alloc-size-attr-sized-by-or-null-sys-header-override.c":{[[@LINE+1]]:27-[[@LINE+1]]:39}:"size * count"
void * __sized_by_or_null(count * size)
    unnamed_param_with_count_flipped(unsigned size, unsigned count) {
    return (void*)0;
}

// expected-error@+1{{'alloc_size' attribute does not match previous declaration}}
__attribute__((alloc_size(2,1)))
    // expected-warning@+1 2{{omitting the parameter name in a function definition is a C23 extension}}
    void * unnamed_param_with_count_mismatching_attrs(unsigned, unsigned) {
    return (void*)0;
}

// expected-warning@+1 2{{omitting the parameter name in a function definition is a C23 extension}}
void * unnamed_param_with_count_mismatching_attrs_after_param_list(unsigned, unsigned)
    // expected-error@+2{{'alloc_size' attribute does not match previous declaration}}
    // expected-warning@+1{{GCC does not allow 'alloc_size' attribute in this position on a function definition}}
    __attribute__((alloc_size(2,1))) {
    return (void*)0;
}

void * override_nullability(unsigned, unsigned)
    __attribute__((returns_nonnull))
    __attribute__((alloc_size(1, 2)));

void * override_nullability2(unsigned, unsigned)
    __attribute__((alloc_size(1, 2)));
