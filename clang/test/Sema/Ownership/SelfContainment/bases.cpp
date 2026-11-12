// RUN: %clang_cc1 -std=c++2b -fsyntax-only \
// RUN:     -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis \
// RUN:     %s -verify -Wownership-inference -Wownership-inference-suggestions

// The outcome of a class is joined with the outcome of each of its bases.
//
// Each record is reported once, no matter how many times it is used as a base.

struct View {                      // expected-warning {{type 'View' was inferred as 'view'}}
  const int *p;                    // expected-note {{holds unmanaged raw-pointer field 'p'}}
};

struct Int { // expected-warning {{type 'Int' was inferred as 'self-contained'}}
  int x;
};

// Deriving from an inferred view makes the derived class a view.
struct DerivesView : View {};
// expected-warning@-1 {{type 'DerivesView' was inferred as 'view'}}
// expected-note@-2 {{derives from class 'View' that was inferred as 'view'}}

// Deriving from a self-contained class keeps the derived class self-contained,
// even though the derived class has no fields of its own.
struct DerivesFromInt : Int {};
// expected-warning@-1 {{type 'DerivesFromInt' was inferred as 'self-contained'}}

// Every base contributes, the worst outcome wins.
// expected-warning@+1 {{type 'View2' was inferred as 'view'}}
struct View2 { 
  int &r; // expected-note {{holds reference field 'r'}}
};
struct Value2 { // expected-warning {{type 'Value2' was inferred as 'self-contained'}}
  int x;
};
struct DerivesBoth : Value2, View2 {};
// expected-warning@-1 {{type 'DerivesBoth' was inferred as 'view'}}
// expected-note@-2 {{derives from class 'View2' that was inferred as 'view'}}

// A base whose inference was inconclusive taints the derived class.
struct Opaque { // expected-warning {{type 'Opaque' failed to infer}}
  int *p;       // expected-note {{holds unmanaged raw-pointer field 'p'}}
  ~Opaque();    // expected-note {{declares opaque destructor}}
};
struct DerivesOpaque : Opaque {};
// expected-warning@-1 {{type 'DerivesOpaque' failed to infer}}
// expected-note@-2 {{derives from class 'Opaque' that was inferred as 'unknown'}}

struct Empty {}; // expected-warning {{type 'Empty' was trivially inferred as 'self-contained'}}
struct DerivesEmpty : Empty {};
// expected-warning@-1 {{type 'DerivesEmpty' was trivially inferred as 'self-contained'}}

struct Value3 { // expected-warning {{type 'Value3' was inferred as 'self-contained'}}
  int x;
};

struct HoldsValue { // expected-warning {{type 'HoldsValue' was inferred as 'view'}}
  Value3 v;
  int &r; // expected-note {{holds reference field 'r'}}
};
