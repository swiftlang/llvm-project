// RUN: %clang_cc1 -fsyntax-only -Wownership-inference -Wownership-inference-suggestions %s -verify

// Fields are looked through arrays, complex, vector and atomic types to find
// the element that decides the outcome.

struct View {   // expected-warning {{type 'View' was inferred as 'view'}}
  const int *p; // expected-note {{holds unmanaged raw-pointer field 'p'}}
};

// Arrays of raw pointers.
struct PtrArray { // expected-warning {{type 'PtrArray' was inferred as 'view'}}
  int *arr[4];    // expected-note {{holds unmanaged raw-pointer field 'arr'}}
};

// Arrays of arrays of raw pointers are unwrapped transitively.
struct PtrArray2D { // expected-warning {{type 'PtrArray2D' was inferred as 'view'}}
  int *arr[2][3];   // expected-note {{holds unmanaged raw-pointer field 'arr'}}
};

// Arrays of views.
struct ViewArray { // expected-warning {{type 'ViewArray' was inferred as 'view'}}
  View arr[2];     // expected-note {{holds field 'arr' of type 'View' that was inferred as 'view'}}
};

// Arrays of references are not valid, but arrays of values are self-contained.
struct IntArray { // expected-warning {{type 'IntArray' was inferred as 'self-contained'}}
  int arr[4];
};

// Complex numbers are self-contained.
struct Complex { // expected-warning {{type 'Complex' was inferred as 'self-contained'}}
  _Complex double c;
  _Complex float cs[2];
};

// Vectors are self-contained.
struct Vector { // expected-warning {{type 'Vector' was inferred as 'self-contained'}}
  int __attribute__((vector_size(16))) v;
};

// Atomics look through to the value type.
struct AtomicInt { // expected-warning {{type 'AtomicInt' was inferred as 'self-contained'}}
  _Atomic(int) a;
};
struct AtomicPtr {  // expected-warning {{type 'AtomicPtr' was inferred as 'view'}}
  _Atomic(int *) a; // expected-note {{holds unmanaged raw-pointer field 'a'}}
};
struct AtomicPtrArray { // expected-warning {{type 'AtomicPtrArray' was inferred as 'view'}}
  _Atomic(int *) a[2];  // expected-note {{holds unmanaged raw-pointer field 'a'}}
};
struct AtomicView {  // expected-warning {{type 'AtomicView' was inferred as 'view'}}
  _Atomic(View *) a; // expected-note {{holds unmanaged raw-pointer field 'a'}}
};

// Pointers to members, function pointers and references to functions don't
// refer to an object, so holding one doesn't make a type a view.

struct S { // expected-warning {{type 'S' was inferred as 'self-contained'}}
  int m;
  void f();
};

// expected-warning@+1 {{type 'HoldsDataMemberPtr' was inferred as 'self-contained'}}
struct HoldsDataMemberPtr {
  int S::*p;
};

// expected-warning@+1 {{type 'HoldsMemberFunctionPtr' was inferred as 'self-contained'}}
struct HoldsMemberFunctionPtr {
  void (S::*f)();
};

// expected-warning@+1 {{type 'HoldsMemberPtrArray' was inferred as 'self-contained'}}
struct HoldsMemberPtrArray {
  int S::*arr[2];
};

// expected-warning@+1 {{type 'HoldsFunctionPtr' was inferred as 'self-contained'}}
struct HoldsFunctionPtr {
  void (*f)();
};

// expected-warning@+1 {{type 'HoldsFunctionRef' was inferred as 'self-contained'}}
struct HoldsFunctionRef {
  void (&f)();
};

// Whereas a reference to an object is still a view.
struct HoldsRef { // expected-warning {{type 'HoldsRef' was inferred as 'view'}}
  int &r; // expected-note {{holds reference field 'r'}}
};
