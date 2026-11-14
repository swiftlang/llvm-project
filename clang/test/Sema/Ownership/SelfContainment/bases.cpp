// RUN: %clang_cc1 -fsyntax-only -Wownership-inference -Wownership-inference-suggestions %s -verify

// The outcome of a class is joined with the outcome of each of its bases.
// Each record is reported once, no matter how many times it is used as a base.

struct ConstIntPtr { // expected-warning {{type 'ConstIntPtr' was inferred as 'view'}}
  const int *p; // expected-note {{holds unmanaged raw-pointer field 'p'}}
};

struct Int { // expected-warning {{type 'Int' was inferred as 'self-contained'}}
  int x;
};

// Deriving from an inferred view makes the derived class a view.
struct DerivesFromConstIntPtr : ConstIntPtr {};
// expected-warning@-1 {{type 'DerivesFromConstIntPtr' was inferred as 'view'}}
// expected-note@-2 {{derives from class 'ConstIntPtr' that was inferred as 'view'}}

// expected-warning@+1 {{type 'DerivesFromInt' was inferred as 'self-contained'}}
struct DerivesFromInt : Int {};

// expected-warning@+1 {{type 'IntRef' was inferred as 'view'}}
struct IntRef {
  int &r; // expected-note {{holds reference field 'r'}}
};
struct Int2 { // expected-warning {{type 'Int2' was inferred as 'self-contained'}}
  int x;
};
struct DerivesFromInt2AndIntRef : Int2, IntRef {};
// expected-warning@-1 {{type 'DerivesFromInt2AndIntRef' was inferred as 'view'}}
// expected-note@-2 {{derives from class 'IntRef' that was inferred as 'view'}}

// A base whose inference was inconclusive taints the derived class.
struct OpaqueIntPtr { // expected-warning {{could not infer the self-containment of type 'OpaqueIntPtr'}}
  int *p; // expected-note {{holds unmanaged raw-pointer field 'p'}}
  ~OpaqueIntPtr(); // expected-note {{declares a destructor whose body is not visible}}
};
struct DerivesFromOpaqueIntPtr : OpaqueIntPtr {};
// expected-warning@-1 {{could not infer the self-containment of type 'DerivesFromOpaqueIntPtr'}}
// expected-note@-2 {{derives from class 'OpaqueIntPtr' that could not be inferred}}

struct Empty {}; // expected-warning {{type 'Empty' was inferred as 'trivially-self-contained'}}
struct DerivesEmpty : Empty {};
// expected-warning@-1 {{type 'DerivesEmpty' was inferred as 'trivially-self-contained'}}

struct Int3 { // expected-warning {{type 'Int3' was inferred as 'self-contained'}}
  int x;
};

struct HoldsInt3 { // expected-warning {{type 'HoldsInt3' was inferred as 'view'}}
  Int3 v;
  int &r; // expected-note {{holds reference field 'r'}}
};

// An inconclusive field type is spelled as such.
struct HoldsOpaqueIntPtr { // expected-warning {{could not infer the self-containment of type 'HoldsOpaqueIntPtr'}}
  OpaqueIntPtr o; // expected-note {{holds field 'o' of type 'OpaqueIntPtr' that could not be inferred}}
};
