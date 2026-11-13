// -Weverything must not enable any of the ownership inference diagnostics, so
// the inference does not even run. Explicitly enabling one of the groups
// enables just that one:
//  - 'none':     nothing is reported.
//  - 'mismatch': only the annotation mismatch warning, and no suggestions.
//  - 'suggest':  only the suggestions, and no annotation mismatch warning even
//                though the inference runs and the annotation is wrong.

// DEFINE: %{flags} = -fsyntax-only -I %S/Inputs -std=c++20 -Wno-c++98-compat \
// DEFINE:   -Wno-c++98-compat-pedantic -Wno-unused -Wno-missing-prototypes -Wno-padded
// RUN: %clang_cc1 %{flags} -Weverything -verify=none %s
// RUN: %clang_cc1 %{flags} -Weverything -Wownership-inference-annotation-mismatch -verify=mismatch %s
// RUN: %clang_cc1 %{flags} -Weverything -Wownership-inference-suggestions -verify=suggest %s

// none-no-diagnostics

#include "RealOwnershipAnnotations.h"

struct Empty {}; // suggest-warning {{type 'Empty' was inferred as 'trivially-self-contained'}}

struct Trivial { // suggest-warning {{type 'Trivial' was inferred as 'self-contained'}}
  int x;
};

struct View { // suggest-warning {{type 'View' was inferred as 'view'}}
  const int *p; // suggest-note {{holds unmanaged raw-pointer field 'p'}}
};
struct Ref { // suggest-warning {{type 'Ref' was inferred as 'view'}}
  int &r; // suggest-note {{holds reference field 'r'}}
};

// mismatch-warning@+2 {{type 'Mismatch' is annotated as 'self-contained' but was inferred as 'view'}}
// suggest-warning@+1 {{type 'Mismatch' was inferred as 'view'}}
struct SWIFT_SELF_CONTAINED Mismatch {
  const int *p; // mismatch-note {{holds unmanaged raw-pointer field 'p'}}
  // suggest-note@-1 {{holds unmanaged raw-pointer field 'p'}}
};
