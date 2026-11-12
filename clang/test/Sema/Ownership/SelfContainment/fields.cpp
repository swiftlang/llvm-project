// RUN: %clang_cc1 -fsyntax-only -Wownership-inference -Wownership-inference-suggestions %s -verify

// Fields are looked through arrays, complex, vector and atomic types to find
// the element that decides the outcome.

struct View {                      // expected-warning {{type 'View' was inferred as 'view'}}
  const int *p;                    // expected-note {{holds unmanaged raw-pointer field 'p'}}
};

// Arrays of raw pointers.
struct PtrArray {                  // expected-warning {{type 'PtrArray' was inferred as 'view'}}
  int *arr[4];                     // expected-note {{holds unmanaged raw-pointer field 'arr'}}
};

// Arrays of arrays of raw pointers are unwrapped transitively.
struct PtrArray2D {                // expected-warning {{type 'PtrArray2D' was inferred as 'view'}}
  int *arr[2][3];                  // expected-note {{holds unmanaged raw-pointer field 'arr'}}
};

// Arrays of views.
struct ViewArray {                 // expected-warning {{type 'ViewArray' was inferred as 'view'}}
  View arr[2];                     // expected-note {{holds field 'arr' of type 'View' that was inferred as 'view'}}
};

// Arrays of references are not valid, but arrays of values are self-contained.
struct IntArray {                  // expected-warning {{type 'IntArray' was inferred as 'self-contained'}}
  int arr[4];
};

// Complex numbers are self-contained.
struct Complex {                   // expected-warning {{type 'Complex' was inferred as 'self-contained'}}
  _Complex double c;
  _Complex float cs[2];
};

// Vectors are self-contained.
typedef int V4SI __attribute__((vector_size(16)));
struct Vector {                    // expected-warning {{type 'Vector' was inferred as 'self-contained'}}
  V4SI v;
};

// Atomics look through to the value type.
struct AtomicInt {                 // expected-warning {{type 'AtomicInt' was inferred as 'self-contained'}}
  _Atomic(int) a;
};
struct AtomicPtr {                 // expected-warning {{type 'AtomicPtr' was inferred as 'view'}}
  _Atomic(int *) a;                // expected-note {{holds unmanaged raw-pointer field 'a'}}
};
struct AtomicPtrArray {            // expected-warning {{type 'AtomicPtrArray' was inferred as 'view'}}
  _Atomic(int *) a[2];             // expected-note {{holds unmanaged raw-pointer field 'a'}}
};
struct AtomicView {                // expected-warning {{type 'AtomicView' was inferred as 'view'}}
  _Atomic(View *) a;               // expected-note {{holds unmanaged raw-pointer field 'a'}}
};

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
