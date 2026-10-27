// Enabling one lifetime-safety warning explicitly starts the analysis, but it
// must not let -Weverything turn on the other lifetime-safety warnings.
// See rdar://190623069.

// RUN: %clang_cc1 -fsyntax-only -std=c++20 -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-unused -Wno-missing-prototypes -Weverything -Wlifetime-safety-experimental-field-origin-escape -verify %s

struct [[gsl::Pointer]] View {
  View(const int &);
  const int *p;
};

struct S {
  View v;
  // expected-warning@+1 {{return value depends on the object referenced by field 'v', not on '*this'}}
  View get() { return v; } // expected-note {{returned here}}
};

// No annotation suggestion.
View suggest(View a) { return a; }
