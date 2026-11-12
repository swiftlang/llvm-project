// RUN: %clang_cc1 -std=c++2b -fsyntax-only \
// RUN:     -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis \
// RUN:     %s -verify=common,on -Wownership-inference -Wownership-inference-suggestions
// RUN: %clang_cc1 -std=c++2b -fsyntax-only \
// RUN:     -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis \
// RUN:     %s -verify=common,off -Wownership-inference -Wno-ownership-inference-annotation-mismatch \
// RUN:     -Wownership-inference-suggestions

// 'on'     additionally expects the annotation mismatch warnings.
// 'off'    disables them, while the other diagnostics are unaffected.
// 'common' is expected either way.

#define SELF_CONTAINED __attribute__((swift_attr("Escapable")))
#define VIEW __attribute__((swift_attr("~Escapable")))

// Classes without fields are trivially self-contained.
struct Empty {};
// common-warning@-1 {{type 'Empty' was trivially inferred as 'self-contained'}}

// An annotation that matches the inference is not diagnosed.
struct SELF_CONTAINED MatchingSelfContained { int x; };
// common-warning@-1 {{type 'MatchingSelfContained' was inferred as 'self-contained'}}
struct VIEW MatchingView { int &r; };
// common-warning@-1 {{type 'MatchingView' was inferred as 'view'}}
// common-note@-2 {{holds reference field 'r'}}

// Self-contained is a different annotation than trivially self-contained.
struct SELF_CONTAINED AnnotatedEmpty {};
// on-warning@-1 {{type 'AnnotatedEmpty' has annotation 'self-contained' but inferred as 'trivially-self-contained'}}
// common-warning@-2 {{type 'AnnotatedEmpty' was trivially inferred as 'self-contained'}}

// Mismatches.
struct SELF_CONTAINED ClaimsSelfContained { int &r; };
// on-warning@-1 {{type 'ClaimsSelfContained' has annotation 'self-contained' but inferred as 'view'}}
// on-note@-2 {{holds reference field 'r'}}
// common-warning@-3 {{type 'ClaimsSelfContained' was inferred as 'view'}}
// common-note@-4 {{holds reference field 'r'}}
struct VIEW ClaimsView { int x; };
// on-warning@-1 {{type 'ClaimsView' has annotation 'view' but inferred as 'self-contained'}}
// common-warning@-2 {{type 'ClaimsView' was inferred as 'self-contained'}}

// An inconclusive inference never conflicts with an annotation.
struct VIEW Inconclusive {         // common-warning {{type 'Inconclusive' failed to infer}}
  int *p;                          // common-note {{holds unmanaged raw-pointer field 'p'}}
  ~Inconclusive();                 // common-note {{declares opaque destructor}}
};

// The annotation of a class is trusted over what the inference would find
// when the class is used as a base or as a field.
struct VIEW AnnotatedView {};
// on-warning@-1 {{type 'AnnotatedView' has annotation 'view' but inferred as 'trivially-self-contained'}}
// common-warning@-2 {{type 'AnnotatedView' was trivially inferred as 'self-contained'}}
struct DerivesAnnotatedView : AnnotatedView {};
// common-warning@-1 {{type 'DerivesAnnotatedView' was inferred as 'view'}}
// common-note@-2 {{derives from class 'AnnotatedView' that was annotated as 'view'}}
struct HoldsAnnotatedView {
  AnnotatedView v;                 // common-note {{holds field 'v' of type 'AnnotatedView' that was annotated as 'view'}}
};
// common-warning@-3 {{type 'HoldsAnnotatedView' was inferred as 'view'}}

struct SELF_CONTAINED AnnotatedSelfContained {
  const int *p;                    // on-note {{holds unmanaged raw-pointer field 'p'}}
                                   // common-note@-1 {{holds unmanaged raw-pointer field 'p'}}
};
// on-warning@-4 {{type 'AnnotatedSelfContained' has annotation 'self-contained' but inferred as 'view'}}
// common-warning@-5 {{type 'AnnotatedSelfContained' was inferred as 'view'}}
struct DerivesAnnotatedSelfContained : AnnotatedSelfContained {};
// common-warning@-1 {{type 'DerivesAnnotatedSelfContained' was inferred as 'self-contained'}}
// common-note@-2 {{derives from class 'AnnotatedSelfContained' that was annotated as 'self-contained'}}
struct HoldsAnnotatedSelfContained {
  AnnotatedSelfContained v;        // common-note {{holds field 'v' of type 'AnnotatedSelfContained' that was annotated as 'self-contained'}}
};
// common-warning@-3 {{type 'HoldsAnnotatedSelfContained' was inferred as 'self-contained'}}
