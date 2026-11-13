// RUN: %clang_cc1 -fsyntax-only -I %S/Inputs -Wownership-inference -Wownership-inference-suggestions %s -verify

#include "RealOwnershipAnnotations.h"

// Classes without fields are trivially self-contained.
struct Empty {};
// expected-warning@-1 {{type 'Empty' was inferred as 'trivially-self-contained'}}

// An annotation that matches the inference is not diagnosed.
struct SWIFT_SELF_CONTAINED MatchingSelfContained { int x; };
// expected-warning@-1 {{type 'MatchingSelfContained' was inferred as 'self-contained'}}

struct SWIFT_VIEW MatchingView { int &r; };
// expected-warning@-1 {{type 'MatchingView' was inferred as 'view'}}
// expected-note@-2 {{holds reference field 'r'}}

// Self-contained is a different annotation than trivially self-contained.
struct SWIFT_SELF_CONTAINED AnnotatedEmpty {};
// expected-warning@-1 {{type 'AnnotatedEmpty' is annotated as 'self-contained' but was inferred as 'trivially-self-contained'}}
// expected-warning@-2 {{type 'AnnotatedEmpty' was inferred as 'trivially-self-contained'}}

// Mismatches.
struct SWIFT_SELF_CONTAINED ClaimsSelfContained { int &r; };
// expected-warning@-1 {{type 'ClaimsSelfContained' is annotated as 'self-contained' but was inferred as 'view'}}
// expected-warning@-2 {{type 'ClaimsSelfContained' was inferred as 'view'}}
// expected-note@-3 {{holds reference field 'r'}}
// expected-note@-4 {{holds reference field 'r'}}

struct SWIFT_VIEW ClaimsView { int x; };
// expected-warning@-1 {{type 'ClaimsView' is annotated as 'view' but was inferred as 'self-contained'}}
// expected-warning@-2 {{type 'ClaimsView' was inferred as 'self-contained'}}

// An inconclusive inference never conflicts with an annotation.
// expected-warning@+1 {{could not infer the self-containment of type 'Inconclusive'}}
struct SWIFT_VIEW Inconclusive {
  int *p;  // expected-note {{holds unmanaged raw-pointer field 'p'}}
  ~Inconclusive(); // expected-note {{declares a destructor whose body is not visible}}
};

// The annotation of a class is trusted over what the inference would find
// when the class is used as a base or as a field.
struct SWIFT_VIEW WronglyAnnotatedView {};
// expected-warning@-1 {{type 'WronglyAnnotatedView' is annotated as 'view' but was inferred as 'trivially-self-contained'}}
// expected-warning@-2 {{type 'WronglyAnnotatedView' was inferred as 'trivially-self-contained'}}

struct DerivesAnnotatedView : WronglyAnnotatedView {};
// expected-warning@-1 {{type 'DerivesAnnotatedView' was inferred as 'view'}}
// expected-note@-2 {{derives from class 'WronglyAnnotatedView' that was annotated as 'view'}}

// expected-warning@+1 {{type 'HoldsAnnotatedView' was inferred as 'view'}}
struct HoldsAnnotatedView {
  WronglyAnnotatedView v; // expected-note {{holds field 'v' of type 'WronglyAnnotatedView' that was annotated as 'view'}}
};

// expected-warning@+2 {{type 'WronglyAnnotatedSelfContained' is annotated as 'self-contained' but was inferred as 'view'}}
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
