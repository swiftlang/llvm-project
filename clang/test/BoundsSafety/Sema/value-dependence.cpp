// RUN: %clang_cc1 -fexperimental-bounds-safety-attributes -verify %s
// RUN: %clang_cc1 -fbounds-safety -fbounds-attributes-cxx-experimental -verify %s

#include <ptrcheck.h>

void * __sized_by_or_null(size) malloc(int size);

int n;
template <typename T>
T bar(int x) {
    // expected-error@+2{{argument of '__sized_by_or_null' attribute cannot refer to declaration from a different scope}}
    // expected-error@+1{{argument of '__sized_by_or_null' attribute cannot refer to declaration of a different lifetime}}
    int * __sized_by_or_null(sizeof(T) * n) p;
    p = static_cast<int*>(malloc(x));
    n = x;
    return *p;
}
