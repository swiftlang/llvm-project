// RUN: %clang_cc1 -fsyntax-only -Wtype-ownership -Wtype-ownership-suggestions %s -verify

// The C counterpart of fields.cpp: plain C records are RecordDecls rather than
// CXXRecordDecls, and must be analyzed without crashing.

// A self-referential pointer field is a raw-pointer field like any other.
// expected-warning@+1 {{type 'SelfPtr' was inferred as 'view'}}
struct SelfPtr {
  struct SelfPtr *next;
  // expected-note@-1 {{holds unmanaged raw-pointer field 'next'}}
};

// Unions are inspected through their members.
// expected-warning@+1 {{type 'UnionPtr' was inferred as 'view'}}
union UnionPtr {
  int i;
  int *p; // expected-note {{holds unmanaged raw-pointer field 'p'}}
};
