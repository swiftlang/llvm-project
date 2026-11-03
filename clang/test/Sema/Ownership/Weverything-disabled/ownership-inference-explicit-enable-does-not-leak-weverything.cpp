// Enabling one ownership-inference warning explicitly starts the self-containment
// inference, but it must not let -Weverything turn on the other ownership-inference
// warnings.

// RUN: %clang_cc1 -fsyntax-only -std=c++20 -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-unused -Wno-missing-prototypes -Wno-padded -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis -Weverything -Wownership-inference-suggest-view -verify %s

#define SWIFT_SELF_CONTAINED __attribute__((swift_attr("import_owned")))

// expected-warning@+1 {{type 'View' was inferred as 'view'}}
struct View {
  const int *p; // expected-note {{holds unmanaged raw-pointer field 'p'}}
};

// No 'trivially self-contained' suggestion.
struct Trivial {
  int x;
};

// Inferred as a view, so only the enabled suggestion; no 'annotation mismatch'
// warning.
// expected-warning@+1 {{type 'Mismatch' was inferred as 'view'}}
struct SWIFT_SELF_CONTAINED Mismatch {
  const int *p; // expected-note {{holds unmanaged raw-pointer field 'p'}}
};
