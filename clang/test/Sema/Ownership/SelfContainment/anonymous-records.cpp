// RUN: %clang_cc1 -fsyntax-only -I %S/Inputs -Wtype-ownership -Wtype-ownership-suggestions %s -verify

#include "RealOwnershipAnnotations.h"

struct ConstIntPtr {   // expected-warning {{type 'ConstIntPtr' was inferred as 'view'}}
  const int *p; // expected-note {{holds unmanaged raw-pointer field 'p'}}
};

struct AnonUnionPtr { // expected-warning {{type 'AnonUnionPtr' was inferred as 'view'}}
  union {
    int i;
    int *p; // expected-note {{holds unmanaged raw-pointer field 'p'}}
  };
};

struct AnonStructRef { // expected-warning {{type 'AnonStructRef' was inferred as 'view'}}
  struct {
    int &r; // expected-note {{holds reference field 'r'}}
  };
};

struct AnonUnionView { // expected-warning {{type 'AnonUnionView' was inferred as 'view'}}
  union {
    int i;
    ConstIntPtr v; // expected-note {{holds field 'v' of type 'ConstIntPtr' that was inferred as 'view'}}
  };
};

// expected-warning@+1 {{type 'AnonUnionSelfContained' was inferred as 'self-contained'}}
struct AnonUnionSelfContained {
  union { int i; float f; };
};

struct Nested { // expected-warning {{type 'Nested' was inferred as 'view'}}
  union {
    struct {
      int *deep; // expected-note {{holds unmanaged raw-pointer field 'deep'}}
    };
    int x;
  };
};

// The raw-pointers of an anonymous member are managed by the destructor of the enclosing record.
// expected-warning@+1 {{could not infer the self-containment of type 'ManagedByEnclosingDtor'}}
struct ManagedByEnclosingDtor {
  union {
    int *p; // expected-note {{holds unmanaged raw-pointer field 'p'}}
    int i;
  };
  ~ManagedByEnclosingDtor(); // expected-note {{declares a destructor whose body is not visible}}
};

struct Opaque { // expected-warning {{could not infer the self-containment of type 'Opaque'}}
  int *p;       // expected-note {{holds unmanaged raw-pointer field 'p'}}
  ~Opaque();    // expected-note {{declares a destructor whose body is not visible}}
};
// expected-warning@+1 {{could not infer the self-containment of type 'AnonUnionOpaque'}}
struct AnonUnionOpaque {
  union {
    int i;
    Opaque o; // expected-note {{holds field 'o' of type 'Opaque' that could not be inferred}}
  };
  ~AnonUnionOpaque();
};

// A named field of an unnamed record type is not an anonymous member:
// the unnamed type is a type on its own, and the field is noted by its name.
struct UnnamedType { // expected-warning {{type 'UnnamedType' was inferred as 'view'}}
  // expected-warning-re@+3 {{type 'UnnamedType::(unnamed struct at {{.*}})' was inferred as 'view'}}
  // expected-note-re@+2 {{holds field 's' of type 'UnnamedType::(unnamed struct at {{.*}})' that was inferred as 'view'}}
  // expected-note@+1 {{holds unmanaged raw-pointer field 'q'}}
  struct { int *q; } s;
};

// expected-warning@+2 {{type 'Annotated' is annotated as 'self-contained' but was inferred as 'view'}}
// expected-warning@+1 {{type 'Annotated' was inferred as 'view'}}
struct SWIFT_SELF_CONTAINED Annotated {
  union {
    int i;
    int *p; // expected-note 2 {{holds unmanaged raw-pointer field 'p'}}
  };
};
