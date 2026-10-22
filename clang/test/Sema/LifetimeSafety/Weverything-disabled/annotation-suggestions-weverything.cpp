// For now, take out the annotation suggestions from -Weverything.
// See rdar://190709787.

// RUN: rm -rf %t
// RUN: split-file %s %t
// DEFINE: %{common} = -fsyntax-only -std=c++20 -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-unused -Wno-dangling -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis -I%t
//
// RUN: %clang_cc1 %{common} -Weverything -verify=none %t/test_source.cpp
// RUN: %clang_cc1 %{common} -Weverything -Werror -Wno-error=lifetime-safety-intra-tu-suggestions -Wno-error=lifetime-safety-cross-tu-suggestions -verify=none %t/test_source.cpp
// RUN: %clang_cc1 %{common} -Weverything -Wlifetime-safety-suggestions -verify=enabled %t/test_source.cpp
// RUN: %clang_cc1 %{common} -Weverything -Wlifetime-safety-intra-tu-suggestions -verify=enabled %t/test_source.cpp
// RUN: %clang_cc1 %{common} -Weverything -Wlifetime-safety-cross-tu-suggestions -verify=enabled %t/test_source.cpp
// RUN: %clang_cc1 %{common} -Weverything -Wlifetime-safety-suggestions \
// RUN:   -Wno-lifetime-safety-suggestions -verify=none %t/test_source.cpp
// RUN: %clang_cc1 %{common} -Wlifetime-safety-suggestions -verify=enabled %t/test_source.cpp

//--- test_header.h
struct [[gsl::Pointer]] View {
  View(const int &);
  const int *p;
};

// Cross-TU param suggestion.
View cross_tu_param(View a); // enabled-warning {{parameter in cross-TU function should be marked [[clang::lifetimebound]]}}

//--- test_source.cpp
// none-no-diagnostics
#include "test_header.h"

View cross_tu_param(View a) { return a; } // enabled-note {{param returned here}}

// Intra-TU param suggestion.
inline View intra_tu_param(View a) { // enabled-warning {{parameter in intra-TU function should be marked [[clang::lifetimebound]]}}
  return a;                          // enabled-note {{param returned here}}
}

// Intra-TU constructor param suggestion.
struct Capture {
  Capture(const int &x) : p(&x) {} // enabled-warning {{parameter in intra-TU constructor should be marked [[clang::lifetimebound]]}}
  const int *p;                    // enabled-note {{escapes to this field}}
};

// Intra-TU 'this' suggestion.
struct S {
  const S &get() const { return *this; } // enabled-warning {{implicit this in intra-TU function should be marked [[clang::lifetimebound]]}}
};                                       // enabled-note@-1 {{param returned here}}
