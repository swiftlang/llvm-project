// RUN: %clang_cc1 -std=c++2b -fsyntax-only \
// RUN:     -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis \
// RUN:     %s -verify -Wownership-inference -Wownership-inference-suggestions

template <class T> void release(T *) __attribute__((ownership_takes(release, 1)));

template <class T> struct Box {    // expected-warning {{type 'Box<char>' failed to infer}}
  T *p;                            // expected-note 2 {{holds unmanaged raw-pointer field 'p'}}
  ~Box() { release(p); }           // expected-note {{destructor was not instantiated for this specialization}}
};

// The destructor of an explicit instantiation has a body. The analysis can't
// yet tell what that body does.
template struct Box<int>;
// expected-warning@-1 {{type 'Box<int>' failed to infer}}

// An implicitly instantiated class whose destructor is never instantiated
// keeps only the declaration, even though the template has a body for it.
// Point that out instead of claiming the destructor is opaque.
unsigned long size = sizeof(Box<char>);

// A destructor that is declared, but defined nowhere, has no body to
// instantiate. Unlike above, there is no pattern body, so it is opaque.
template <class T> struct DeclaredOnly { // expected-warning {{type 'DeclaredOnly<char>' failed to infer}}
  T *p;                            // expected-note {{holds unmanaged raw-pointer field 'p'}}
  ~DeclaredOnly();                 // expected-note {{declares opaque destructor}}
};
unsigned long size2 = sizeof(DeclaredOnly<char>);
