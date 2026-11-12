// RUN: %clang_cc1 -std=c++2b -fsyntax-only \
// RUN:     -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis \
// RUN:     %s -verify -Wownership-inference -Wownership-inference-suggestions

// The outcome of a class is joined with the outcome of each of its bases.
//
// Each record is reported once, no matter how many times it is used as a base.

struct View {                      // expected-warning {{type 'View' was inferred as 'view'}}
  const int *p;                    // expected-note {{holds unmanaged raw-pointer field 'p'}}
};

struct Value {                     // expected-warning {{type 'Value' was inferred as 'self-contained'}}
  int x;
};

// Deriving from an inferred view makes the derived class a view.
struct DerivesView : View {};
// expected-warning@-1 {{type 'DerivesView' was inferred as 'view'}}
// expected-note@-2 {{derives from class 'View' that was inferred as 'view'}}

// Deriving from a self-contained class keeps the derived class self-contained,
// even though the derived class has no fields of its own.
struct DerivesValue : Value {};
// expected-warning@-1 {{type 'DerivesValue' was inferred as 'self-contained'}}
// expected-note@-2 {{derives from class 'Value' that was inferred as 'self-contained'}}

// Every base contributes, the worst outcome wins.
struct View2 {                     // expected-warning {{type 'View2' was inferred as 'view'}}
  int &r;                          // expected-note {{holds reference field 'r'}}
};
struct Value2 {                    // expected-warning {{type 'Value2' was inferred as 'self-contained'}}
  int x;
};
struct DerivesBoth : Value2, View2 {};
// expected-warning@-1 {{type 'DerivesBoth' was inferred as 'view'}}
// expected-note@-2 {{derives from class 'Value2' that was inferred as 'self-contained'}}
// expected-note@-3 {{derives from class 'View2' that was inferred as 'view'}}

// A base whose inference was inconclusive taints the derived class.
struct Opaque {                    // expected-warning {{type 'Opaque' failed to infer}}
  int *p;                          // expected-note {{holds unmanaged raw-pointer field 'p'}}
  ~Opaque();                       // expected-note {{declares opaque destructor}}
};
struct DerivesOpaque : Opaque {};
// expected-warning@-1 {{type 'DerivesOpaque' failed to infer}}
// expected-note@-2 {{derives from class 'Opaque' that was inferred as 'unknown'}}

// The annotation of a base is trusted over what the inference would find.
struct __attribute__((swift_attr("~Escapable"))) AnnotatedView {};
// expected-warning@-1 {{type 'AnnotatedView' has annotation 'view' but inferred as 'trivially-self-contained'}}
// expected-warning@-2 {{type 'AnnotatedView' was trivially inferred as 'self-contained'}}
struct DerivesAnnotatedView : AnnotatedView {};
// expected-warning@-1 {{type 'DerivesAnnotatedView' was inferred as 'view'}}
// expected-note@-2 {{derives from class 'AnnotatedView' that was annotated as 'view'}}

struct __attribute__((swift_attr("Escapable"))) AnnotatedSelfContained {
  const int *p;                    // expected-note 2 {{holds unmanaged raw-pointer field 'p'}}
};
// expected-warning@-3 {{type 'AnnotatedSelfContained' has annotation 'self-contained' but inferred as 'view'}}
// expected-warning@-4 {{type 'AnnotatedSelfContained' was inferred as 'view'}}
struct DerivesAnnotatedSelfContained : AnnotatedSelfContained {};
// expected-warning@-1 {{type 'DerivesAnnotatedSelfContained' was inferred as 'self-contained'}}
// expected-note@-2 {{derives from class 'AnnotatedSelfContained' that was annotated as 'self-contained'}}

// The same applies for fields holding annotated classes.
struct HoldsAnnotatedView {
  AnnotatedView v;                 // expected-note {{holds field 'v' of type 'AnnotatedView' that was annotated as 'view'}}
};
// expected-warning@-3 {{type 'HoldsAnnotatedView' was inferred as 'view'}}

// The notes spell the outcome of the base or field, whatever it is.
struct Empty {};                   // expected-warning {{type 'Empty' was trivially inferred as 'self-contained'}}
struct DerivesEmpty : Empty {};
// expected-warning@-1 {{type 'DerivesEmpty' was trivially inferred as 'self-contained'}}
// expected-note@-2 {{derives from class 'Empty' that was inferred as 'trivially-self-contained'}}

struct Value3 {                    // expected-warning {{type 'Value3' was inferred as 'self-contained'}}
  int x;
};
struct HoldsValue {                // expected-warning {{type 'HoldsValue' was inferred as 'view'}}
  Value3 v;                        // expected-note {{holds field 'v' of type 'Value3' that was inferred as 'self-contained'}}
  int &r;                          // expected-note {{holds reference field 'r'}}
};

struct HoldsAnnotatedSelfContained {
  AnnotatedSelfContained v;        // expected-note {{holds field 'v' of type 'AnnotatedSelfContained' that was annotated as 'self-contained'}}
};
// expected-warning@-3 {{type 'HoldsAnnotatedSelfContained' was inferred as 'self-contained'}}
