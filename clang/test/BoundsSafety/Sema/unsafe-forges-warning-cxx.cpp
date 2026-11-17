// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -fexperimental-bounds-safety-cxx -Wbounds-safety-unsafe-forges -verify %s
#include <ptrcheck.h>

// Warned once at the template definition, regardless of instantiation.
template <typename T>
T *forge(T *p) {
  // expected-warning@+1{{use of '__unsafe_forge_single' is unsafe}}
  return (T *)__builtin_unsafe_forge_single(p);
}

template <typename T>
void never_instantiated(T *p) {
  // expected-warning@+1{{use of '__unsafe_forge_single' is unsafe}}
  (void)__builtin_unsafe_forge_single(p);
}

void use(int *p, char *q) {
  forge(p);
  forge(q);
}
