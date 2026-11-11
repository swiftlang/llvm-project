// RUN: %clang_cc1 -fsyntax-only \
// RUN:     -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis \
// RUN:     -Wlifetime-safety-experimental-field-origin-escape \
// RUN:     %s -verify -Wownership-inference -Wownership-inference-suggestions

// expected-warning@+1 {{type 'D1' was inferred as 'view'}}
struct D1 {
  int *p; // expected-note {{holds unmanaged raw-pointer field 'p'}}
  ~D1() = default;
};

// expected-warning@+1 {{type 'D2' was inferred as 'view'}}
struct D2 {
  int *p; // expected-note {{holds unmanaged raw-pointer field 'p'}}
  ~D2();  // defaulted out-of-line
};
D2::~D2() = default;

// expected-warning@+1 {{type 'D3' failed to infer}}
struct D3 {
  int *p; // expected-note {{holds unmanaged raw-pointer field 'p'}}
  ~D3();  // expected-note {{declares opaque destructor}}
};
