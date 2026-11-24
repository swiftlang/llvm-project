// RUN: %clang_cc1 -fsyntax-only -Wthread-safety -verify %s

// A late-parsed attribute written before a parameter's type is parsed where it
// is written, as upstream does, even though the parameter's declarator passes
// a late-parsed attribute list to its decl-spec for -fbounds-safety.

struct __attribute__((capability("mutex"))) Mutex {};

struct S {
  void r(__attribute__((requires_capability(mu))) void (*cb)()); // expected-error {{use of undeclared identifier 'mu'}}
  Mutex mu;
};

void g(__attribute__((requires_capability(m))) void (*cb)(), Mutex &m); // expected-error {{use of undeclared identifier 'm'}}

struct T {
  void f(int x);
};
void T::f(__attribute__((diagnose_if(x > 0, "bad", "warning"))) int x) {} // expected-error {{use of undeclared identifier 'x'}}
