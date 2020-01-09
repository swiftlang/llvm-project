// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

typedef long long size_t;

typedef struct __sFILE {
  unsigned char *_p;
  int _w;
  int _lbfsize;
} FILE;

// In order to evaluate potentially constant expressions, the interpreter
// creates a dummy pointer to an external block which cannot be read, but
// supports pointer derivations.
int __sputc(int _c, FILE *_p) {
  if (--_p->_w >= 0)
    return 1;
  return 0;
}
