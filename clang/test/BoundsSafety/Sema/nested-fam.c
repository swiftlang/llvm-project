// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -verify %s
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -x objective-c -fexperimental-bounds-safety-objc -verify %s
#include <ptrcheck.h>

typedef struct {
    int count;
    char inner_arr[__counted_by(count)];
} InnerFam;

typedef struct {
    int count;
    // expected-error@+1{{'counted_by' cannot be applied to an array with element of unknown size because 'InnerFam' is a struct type with a flexible array member}}
    InnerFam outer_arr[__counted_by(count)];
} OuterFam;


typedef struct {
    int count;
    // expected-error@+1{{cannot be applied to a pointer with pointee of unknown size because 'InnerFam' is a struct type with a flexible array member}}
    InnerFam*__counted_by(count) outer;
} OuterPtr;
