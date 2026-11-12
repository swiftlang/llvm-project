// RUN: %clang_cc1 -fsyntax-only -I %S/Inputs -Wownership-inference -Wownership-inference-suggestions %s -verify

#include "RealOwnershipAnnotations.h"

// Classes without fields are trivially self-contained.
struct Empty {};
// expected-warning@-1 {{type 'Empty' was trivially inferred as 'self-contained'}}

// An annotation that matches the inference is not diagnosed.
struct SWIFT_SELF_CONTAINED MatchingSelfContained { int x; };
// expected-warning@-1 {{type 'MatchingSelfContained' was inferred as 'self-contained'}}

struct SWIFT_VIEW MatchingView { int &r; };
// expected-warning@-1 {{type 'MatchingView' was inferred as 'view'}}
// expected-note@-2 {{holds reference field 'r'}}

// Self-contained is a different annotation than trivially self-contained.
struct SWIFT_SELF_CONTAINED AnnotatedEmpty {};
// expected-warning@-1 {{type 'AnnotatedEmpty' has annotation 'self-contained' but inferred as 'trivially-self-contained'}}
// expected-warning@-2 {{type 'AnnotatedEmpty' was trivially inferred as 'self-contained'}}

// Mismatches.
struct SWIFT_SELF_CONTAINED ClaimsSelfContained { int &r; };
// expected-warning@-1 {{type 'ClaimsSelfContained' has annotation 'self-contained' but inferred as 'view'}}
// expected-warning@-2 {{type 'ClaimsSelfContained' was inferred as 'view'}}
// expected-note@-3 {{holds reference field 'r'}}
// expected-note@-4 {{holds reference field 'r'}}

struct SWIFT_VIEW ClaimsView { int x; };
// expected-warning@-1 {{type 'ClaimsView' has annotation 'view' but inferred as 'self-contained'}}
// expected-warning@-2 {{type 'ClaimsView' was inferred as 'self-contained'}}

// An inconclusive inference never conflicts with an annotation.
// expected-warning@+1 {{type 'Inconclusive' failed to infer}}
struct SWIFT_VIEW Inconclusive {
  int *p;  // expected-note {{holds unmanaged raw-pointer field 'p'}}
  ~Inconclusive(); // expected-note {{declares opaque destructor}}
};

// The annotation of a class is trusted over what the inference would find
// when the class is used as a base or as a field.
struct SWIFT_VIEW WronglyAnnotatedView {};
// expected-warning@-1 {{type 'WronglyAnnotatedView' has annotation 'view' but inferred as 'trivially-self-contained'}}
// expected-warning@-2 {{type 'WronglyAnnotatedView' was trivially inferred as 'self-contained'}}

struct DerivesAnnotatedView : WronglyAnnotatedView {};
// expected-warning@-1 {{type 'DerivesAnnotatedView' was inferred as 'view'}}
// expected-note@-2 {{derives from class 'WronglyAnnotatedView' that was annotated as 'view'}}

// expected-warning@+1 {{type 'HoldsAnnotatedView' was inferred as 'view'}}
struct HoldsAnnotatedView {
  WronglyAnnotatedView v; // expected-note {{holds field 'v' of type 'WronglyAnnotatedView' that was annotated as 'view'}}
};

// expected-warning@+2 {{type 'WronglyAnnotatedSelfContained' has annotation 'self-contained' but inferred as 'view'}}
// expected-warning@+1 {{type 'WronglyAnnotatedSelfContained' was inferred as 'view'}}
struct SWIFT_SELF_CONTAINED WronglyAnnotatedSelfContained {
  const int *p;
  // expected-note@-1 {{holds unmanaged raw-pointer field 'p'}}
  // expected-note@-2 {{holds unmanaged raw-pointer field 'p'}}
};

// expected-warning@+1 {{type 'DerivesAnnotatedSelfContained' was inferred as 'self-contained'}}
struct DerivesAnnotatedSelfContained : WronglyAnnotatedSelfContained {};

// expected-warning@+1 {{type 'HoldsAnnotatedSelfContained' was inferred as 'self-contained'}}
struct HoldsAnnotatedSelfContained {
  WronglyAnnotatedSelfContained v;
};
