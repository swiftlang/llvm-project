// RUN: %clang_cc1 -fsyntax-only -Wownership-inference -Wownership-inference-suggestions %s -verify

// Init-capture: named after the synthesized init-capture variable.
void mkInit() { (void)[ptr = (int *)0] { return ptr; }; }
// expected-warning-re@-1 {{type '(lambda at {{.*}})' was inferred as 'view'}}
// expected-note@-2 {{holds unmanaged raw-pointer field 'ptr'}}

// By-value capture: named after the captured variable.
void mkVal(int *p) { (void)[p] { return p; }; }
// expected-warning-re@-1 {{type '(lambda at {{.*}})' was inferred as 'view'}}
// expected-note@-2 {{holds unmanaged raw-pointer field 'p'}}

// By-reference capture: the capture field is a reference, named after the var.
void mkRef(int &x) { (void)[&x] { return &x; }; }
// expected-warning-re@-1 {{type '(lambda at {{.*}})' was inferred as 'view'}}
// expected-note@-2 {{holds reference field 'x'}}

// `this`-capture: no backing variable, so the note spells 'this'.
struct S { // expected-warning {{type 'S' was inferred as 'self-contained'}}
  int v;
  void mkThis() { (void)[this] { return v; }; }
  // expected-warning-re@-1 {{type '(lambda at {{.*}})' was inferred as 'view'}}
  // expected-note@-2 {{holds unmanaged raw-pointer field 'this'}}
};

// expected-warning-re@+1 {{type '(lambda at {{.*}})' was inferred as 'view'}}
void mkMany(int a, int *p, int &r) { (void)[a, p, &r] { return a + *p + r; }; }
// 'a' is not mentioned because it's self-contained.
// expected-note@-2 {{holds unmanaged raw-pointer field 'p'}}
// expected-note@-3 {{holds reference field 'r'}}
