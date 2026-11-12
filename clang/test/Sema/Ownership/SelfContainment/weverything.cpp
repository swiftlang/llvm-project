// Explicitly enabling -Wownership-inference-annotation-mismatch enables that specific warning.
// -Weverything shouldn't enable -Wownership-inference-suggestions.

// RUN: %clang_cc1 -fsyntax-only -std=c++20 -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-unused -Wno-missing-prototypes -Wno-padded \
// RUN:   -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis -Weverything \
// RUN:   -Wownership-inference-annotation-mismatch -verify %s

#define SWIFT_SELF_CONTAINED __attribute__((swift_attr("Escapable")))

// No 'view' suggestion.
struct View {
  const int *p;
};

// No 'trivially self-contained' suggestion.
struct Trivial {
  int x;
};

// Inferred as a view, so only the enabled suggestion; no 'annotation mismatch' warning.
// expected-warning@+1 {{type 'Mismatch' has annotation 'self-contained' but inferred as 'view'}}
struct SWIFT_SELF_CONTAINED Mismatch {
  const int *p; // expected-note {{holds unmanaged raw-pointer field 'p'}}
};
