// For now, take out this warning from -Weverything. See rdar://190623069

// DEFINE: %{common} = -fsyntax-only -std=c++20 -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-unused
//
// RUN: %clang_cc1 %{common} -Weverything -verify=none %s
// RUN: %clang_cc1 %{common} -Weverything -Wlifetime-safety-experimental-field-origin-escape -verify=enabled %s
// RUN: %clang_cc1 %{common} -Weverything -Wlifetime-safety-experimental -verify=enabled %s
// RUN: %clang_cc1 %{common} -Weverything \
// RUN:      -Wlifetime-safety-experimental-field-origin-escape \
// RUN:   -Wno-lifetime-safety-experimental-field-origin-escape -verify=none %s
// RUN: %clang_cc1 %{common} -Wlifetime-safety-experimental-field-origin-escape -verify=enabled %s

// none-no-diagnostics

struct [[gsl::Pointer]] View {
  const int *p;
};

struct S {
  View v;
  // enabled-warning@+1 {{return value depends on the object referenced by field 'v', not on '*this'}}
  View get() { return v; } // enabled-note {{returned here}}
};
