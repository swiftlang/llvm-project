// RUN: %clang_cc1 -std=c++2b -fsyntax-only \
// RUN:     -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis \
// RUN:     %s -verify=on -Wownership-inference -Wownership-inference-suggestions
// RUN: %clang_cc1 -std=c++2b -fsyntax-only \
// RUN:     -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis \
// RUN:     %s -verify=off -Wownership-inference -Wno-ownership-inference-annotation-mismatch \
// RUN:     -Wownership-inference-suggestions

// 'on'  expects the mismatch warnings.
// 'off' expects none of them, while the other diagnostics are unaffected.

#define SELF_CONTAINED __attribute__((swift_attr("Escapable")))
#define VIEW __attribute__((swift_attr("~Escapable")))

// Classes without fields are trivially self-contained.
struct Empty {};
// on-warning@-1 {{type 'Empty' was trivially inferred as 'self-contained'}}
// off-warning@-2 {{type 'Empty' was trivially inferred as 'self-contained'}}

// An annotation that matches the inference is not diagnosed.
struct SELF_CONTAINED MatchingSelfContained { int x; };
// on-warning@-1 {{type 'MatchingSelfContained' was inferred as 'self-contained'}}
// off-warning@-2 {{type 'MatchingSelfContained' was inferred as 'self-contained'}}
struct VIEW MatchingView { int &r; };
// on-warning@-1 {{type 'MatchingView' was inferred as 'view'}}
// on-note@-2 {{holds reference field 'r'}}
// off-warning@-3 {{type 'MatchingView' was inferred as 'view'}}
// off-note@-4 {{holds reference field 'r'}}

// Self-contained is a different annotation than trivially self-contained.
struct SELF_CONTAINED AnnotatedEmpty {};
// on-warning@-1 {{type 'AnnotatedEmpty' has annotation 'self-contained' but inferred as 'trivially-self-contained'}}
// on-warning@-2 {{type 'AnnotatedEmpty' was trivially inferred as 'self-contained'}}
// off-warning@-3 {{type 'AnnotatedEmpty' was trivially inferred as 'self-contained'}}

// Mismatches.
struct SELF_CONTAINED ClaimsSelfContained { int &r; };
// on-warning@-1 {{type 'ClaimsSelfContained' has annotation 'self-contained' but inferred as 'view'}}
// on-note@-2 {{holds reference field 'r'}}
// on-warning@-3 {{type 'ClaimsSelfContained' was inferred as 'view'}}
// on-note@-4 {{holds reference field 'r'}}
// off-warning@-5 {{type 'ClaimsSelfContained' was inferred as 'view'}}
// off-note@-6 {{holds reference field 'r'}}
struct VIEW ClaimsView { int x; };
// on-warning@-1 {{type 'ClaimsView' has annotation 'view' but inferred as 'self-contained'}}
// on-warning@-2 {{type 'ClaimsView' was inferred as 'self-contained'}}
// off-warning@-3 {{type 'ClaimsView' was inferred as 'self-contained'}}

// An inconclusive inference never conflicts with an annotation.
struct VIEW Inconclusive {         // on-warning {{type 'Inconclusive' failed to infer}}
                                   // off-warning@-1 {{type 'Inconclusive' failed to infer}}
  int *p;                          // on-note {{holds unmanaged raw-pointer field 'p'}}
                                   // off-note@-1 {{holds unmanaged raw-pointer field 'p'}}
  ~Inconclusive();                 // on-note {{declares opaque destructor}}
                                   // off-note@-1 {{declares opaque destructor}}
};
