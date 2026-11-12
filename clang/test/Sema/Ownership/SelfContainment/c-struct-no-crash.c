// RUN: %clang_cc1 -fsyntax-only \
// RUN:     -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis \
// RUN:     -Wlifetime-safety-experimental-field-origin-escape \
// RUN:     %s -verify -Wownership-inference -Wownership-inference-suggestions

// expected-warning@+1 {{type 'RawPtr' was inferred as 'view'}}
struct RawPtr {
  int *p; // expected-note {{holds unmanaged raw-pointer field 'p'}}
};

// expected-warning@+1 {{type 'SelfRef' was inferred as 'view'}}
struct SelfRef {
  struct SelfRef *next; // expected-note {{holds unmanaged raw-pointer field 'next'}}
};

// expected-warning@+1 {{type 'UnionPtr' was inferred as 'view'}}
union UnionPtr {
  int i;
  int *p; // expected-note {{holds unmanaged raw-pointer field 'p'}}
};

// expected-warning@+1 {{type 'TrivialC' was inferred as 'self-contained'}}
struct TrivialC {
  int x;
};

// expected-warning@+1 {{type 'FuncPtrOnly' was inferred as 'self-contained'}}
struct FuncPtrOnly {
  void (*fn)(void);
};
