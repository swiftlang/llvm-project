// RUN: %clang_cc1 -fsyntax-only -Wownership-inference -Wownership-inference-suggestions %s -verify

// Holding a closure by value forces the analysis to visit the closure type
// (via field folding); the closure is inferred 'view' and the note names the capture.
template <class L> struct Holder { L lam; };
// expected-note@-1 5 {{holds field 'lam' of type}}

// Init-capture: named after the synthesized init-capture variable.
auto mkInit() { return [ptr = (int *)0] { return ptr; }; }
// expected-warning-re@-1 {{type '(lambda at {{.*}})' was inferred as 'view'}}
// expected-note@-2 {{holds unmanaged raw-pointer field 'ptr'}}
template struct Holder<decltype(mkInit())>;
// expected-warning-re@-1 {{type 'Holder<(lambda at {{.*}})>' was inferred as 'view'}}

// By-value capture: named after the captured variable.
auto mkVal(int *p) { return [p] { return p; }; }
// expected-warning-re@-1 {{type '(lambda at {{.*}})' was inferred as 'view'}}
// expected-note@-2 {{holds unmanaged raw-pointer field 'p'}}
template struct Holder<decltype(mkVal(nullptr))>;
// expected-warning-re@-1 {{type 'Holder<(lambda at {{.*}})>' was inferred as 'view'}}

// By-reference capture: the capture field is a reference, named after the var.
auto mkRef(int &x) { return [&x] { return &x; }; }
// expected-warning-re@-1 {{type '(lambda at {{.*}})' was inferred as 'view'}}
// expected-note@-2 {{holds reference field 'x'}}
template struct Holder<decltype(mkRef(*(int *)0))>;
// expected-warning-re@-1 {{type 'Holder<(lambda at {{.*}})>' was inferred as 'view'}}

// `this`-capture: no backing variable, so the note spells 'this'.
struct S {
  // expected-warning@-1 {{type 'S' was inferred as 'self-contained'}}
  int v;
  auto mkThis() { return [this] { return v; }; }
  // expected-warning-re@-1 {{type '(lambda at {{.*}})' was inferred as 'view'}}
  // expected-note@-2 {{holds unmanaged raw-pointer field 'this'}}
};
template struct Holder<decltype(((S *)0)->mkThis())>;
// expected-warning-re@-1 {{type 'Holder<(lambda at {{.*}})>' was inferred as 'view'}}

// Several captures: the note names the capture matching the offending field,
// not the first one.
auto mkMany(int a, int *p, int &r) { return [a, p, &r] { return a + *p + r; }; }
// expected-warning-re@-1 {{type '(lambda at {{.*}})' was inferred as 'view'}}
// expected-note@-2 {{holds unmanaged raw-pointer field 'p'}}
// expected-note@-3 {{holds reference field 'r'}}
template struct Holder<decltype(mkMany(0, nullptr, *(int *)0))>;
// expected-warning-re@-1 {{type 'Holder<(lambda at {{.*}})>' was inferred as 'view'}}
