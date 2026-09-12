// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -verify %s
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -x objective-c -fexperimental-bounds-safety-objc -verify %s
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -Wno-error=bounds-safety-counted-by-elt-type-unknown-size -verify=downgrade %s
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -Wno-error=bounds-safety-counted-by-elt-type-unknown-size -x objective-c -fexperimental-bounds-safety-objc -verify=downgrade %s
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -Wno-bounds-safety-counted-by-elt-type-unknown-size -verify=suppress %s
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -Wno-bounds-safety-counted-by-elt-type-unknown-size -x objective-c -fexperimental-bounds-safety-objc -verify=suppress %s
#include <ptrcheck.h>

typedef struct {
    int count;
    char inner_arr[__counted_by(count)];
} InnerFam;

// For FAMs the diagnostic is an error by default but can be downgraded to a warning or suppressed completely.
typedef struct {
    int count;
    // suppressed for `-Wno-bounds-safety-counted-by-elt-type-unknown-size`
    // downgrade-warning@+2{{'counted_by' should not be applied to an array with element of unknown size because 'InnerFam' is a struct type with a flexible array member. This will be an error in a future compiler version}}
    // expected-error@+1{{'counted_by' should not be applied to an array with element of unknown size because 'InnerFam' is a struct type with a flexible array member. This will be an error in a future compiler version}}
    InnerFam outer_arr[__counted_by(count)];
} OuterFam;

// For counted_by pointers the diagnostic is always an error.
typedef struct {
    int count;
    // suppress-error@+3{{'counted_by' cannot be applied to a pointer with pointee of unknown size because 'InnerFam' is a struct type with a flexible array member}}
    // downgrade-error@+2{{'counted_by' cannot be applied to a pointer with pointee of unknown size because 'InnerFam' is a struct type with a flexible array member}}
    // expected-error@+1{{'counted_by' cannot be applied to a pointer with pointee of unknown size because 'InnerFam' is a struct type with a flexible array member}}
    InnerFam*__counted_by(count) outer;
} OuterPtrCb;

typedef struct {
    int count;
    // suppress-error@+3{{'counted_by_or_null' cannot be applied to a pointer with pointee of unknown size because 'InnerFam' is a struct type with a flexible array member}}
    // downgrade-error@+2{{'counted_by_or_null' cannot be applied to a pointer with pointee of unknown size because 'InnerFam' is a struct type with a flexible array member}}
    // expected-error@+1{{'counted_by_or_null' cannot be applied to a pointer with pointee of unknown size because 'InnerFam' is a struct type with a flexible array member}}
    InnerFam*__counted_by_or_null(count) outer;
} OuterPtrCbon;

// Ok for sized_by, sized_by_or_null, ended_by
typedef struct {
    int size;
    InnerFam*__sized_by(size) outer;
} OuterPtrSb;

typedef struct {
    int size;
    InnerFam*__sized_by_or_null(size) outer;
} OuterPtrSbon;

typedef struct {
    InnerFam* end;
    InnerFam*__ended_by(end) start;
} OuterPtrEb;
