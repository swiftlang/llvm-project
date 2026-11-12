// RUN: %clang_cc1 -fsyntax-only -Wownership-inference -Wownership-inference-suggestions %s -verify

// expected-warning@+1 {{type 'SelfPtr' was inferred as 'view'}}
struct SelfPtr {
  struct SelfPtr *next;
  // expected-note@-1 {{holds unmanaged raw-pointer field 'next'}}
};

// expected-warning@+1 {{type 'UnionPtr' was inferred as 'view'}}
union UnionPtr {
  int i;
  int *p; // expected-note {{holds unmanaged raw-pointer field 'p'}}
};
