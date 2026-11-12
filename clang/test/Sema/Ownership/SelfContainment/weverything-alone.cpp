// RUN: %clang_cc1 -fsyntax-only -std=c++20 -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-unused -Wno-missing-prototypes -Wno-padded \
// RUN:   -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis -Weverything \
// RUN:   -verify %s

// -Weverything must not enable any of the ownership inference diagnostics, so
// the inference does not even run.

// expected-no-diagnostics
struct Empty {};
struct View { const int *p; };
struct Ref { int &r; };
struct __attribute__((swift_attr("Escapable"))) Mismatch { const int *p; };
