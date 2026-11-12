// -Weverything must not enable any of the ownership inference diagnostics, so
// the inference does not even run. Explicitly enabling one of the groups
// enables just that one, and -Weverything doesn't enable the suggestions.

// DEFINE: %{flags} = -fsyntax-only -I %S/Inputs -std=c++20 -Wno-c++98-compat \
// DEFINE:   -Wno-c++98-compat-pedantic -Wno-unused -Wno-missing-prototypes -Wno-padded
// RUN: %clang_cc1 %{flags} -Weverything -verify=none %s
// RUN: %clang_cc1 %{flags} -Weverything -Wownership-inference-annotation-mismatch -verify=mismatch %s

// none-no-diagnostics

#include "RealOwnershipAnnotations.h"

// No 'trivially self-contained' suggestion.
struct Empty {};

// No 'self-contained' suggestion.
struct Trivial {
  int x;
};

// No 'view' suggestion.
struct View {
  const int *p;
};
struct Ref {
  int &r;
};

// Inferred as a view, so only the enabled warning and no 'view' suggestion.
// mismatch-warning@+1 {{type 'Mismatch' has annotation 'self-contained' but inferred as 'view'}}
struct SWIFT_SELF_CONTAINED Mismatch {
  const int *p; // mismatch-note {{holds unmanaged raw-pointer field 'p'}}
};
