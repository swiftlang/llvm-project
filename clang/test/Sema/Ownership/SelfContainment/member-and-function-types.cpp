// RUN: %clang_cc1 -std=c++2b -fsyntax-only \
// RUN:     -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis \
// RUN:     %s -verify -Wownership-inference -Wownership-inference-suggestions

// Pointers to members, function pointers and references to functions don't
// refer to an object, so holding one doesn't make a type a view.

struct S {                         // expected-warning {{type 'S' was inferred as 'self-contained'}}
  int m;
  void f();
};

struct HoldsDataMemberPtr {        // expected-warning {{type 'HoldsDataMemberPtr' was inferred as 'self-contained'}}
  int S::*p;
};

struct HoldsMemberFunctionPtr {    // expected-warning {{type 'HoldsMemberFunctionPtr' was inferred as 'self-contained'}}
  void (S::*f)();
};

struct HoldsMemberPtrArray {       // expected-warning {{type 'HoldsMemberPtrArray' was inferred as 'self-contained'}}
  int S::*arr[2];
};

struct HoldsFunctionPtr {          // expected-warning {{type 'HoldsFunctionPtr' was inferred as 'self-contained'}}
  void (*f)();
};

struct HoldsFunctionRef {          // expected-warning {{type 'HoldsFunctionRef' was inferred as 'self-contained'}}
  void (&f)();
};

// Whereas a reference to an object is still a view.
struct HoldsRef {                  // expected-warning {{type 'HoldsRef' was inferred as 'view'}}
  int &r;                          // expected-note {{holds reference field 'r'}}
};
