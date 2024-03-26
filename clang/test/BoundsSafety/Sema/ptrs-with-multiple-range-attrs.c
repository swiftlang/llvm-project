// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -verify=expected,c %s
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -x objective-c -fexperimental-bounds-safety-objc -verify=expected,c %s
// RUN: %clang_cc1 -fsyntax-only -fexperimental-bounds-safety-attributes -x c -verify=expected,c %s
// RUN: %clang_cc1 -fsyntax-only -fexperimental-bounds-safety-attributes -x c++ -verify=expected,cxx %s
// RUN: %clang_cc1 -fsyntax-only -fexperimental-bounds-safety-attributes -x objective-c -verify=expected,c %s
// RUN: %clang_cc1 -fsyntax-only -fexperimental-bounds-safety-attributes -x objective-c++ -verify=expected,cxx %s

#include <ptrcheck.h>

void foo(int *__counted_by(len) __counted_by(len+2)* buf, int len); // expected-error{{pointer cannot have more than one count attribute}}
                                                                    // expected-note@-1{{conflicting attributes were '__counted_by(len)' and '__counted_by(len + 2)'}}
                                                                    // expected-note@-2{{previous attribute is here}}
void bar(int **__counted_by(len) buf __counted_by(len + 1), int len); // expected-error{{pointer cannot have more than one count attribute}}
                                                                      // expected-note@-1{{conflicting attributes were '__counted_by(len + 1)' and '__counted_by(len)'}}
                                                                      // expected-note@-2{{previous attribute is here}}
int *__counted_by(len) __counted_by(len + 2) baz(int len); // expected-error{{pointer cannot have more than one count attribute}}
                                                           // expected-note@-1{{conflicting attributes were '__counted_by(len)' and '__counted_by(len + 2)'}}
                                                           // expected-note@-2{{previous attribute is here}}

int *__counted_by(len) bazx(int len);
int *__counted_by(len) bazx(int len); // ok - same count expr
int *__counted_by(len) bazx(int len) {} // ok - same count expr

int *__counted_by(4) bazz();
int *__counted_by(4) bazz(); // ok - same count expr
int *__counted_by(4) bazz() {} // ok - same count expr

// expected-note@+1{{previous declaration is here}}
int *bazy();
// expected-error@+1{{conflicting '__counted_by' attribute with the previous function declaration}}
int *__counted_by(4) bazy() {} // ok

// expected-note@+1 2{{previous declaration is here}}
int *__counted_by(len) bayz(unsigned len);
// expected-error@+1{{conflicting '__counted_by' attribute with the previous function declaration}}
int *bayz(unsigned len) {}
// expected-error@+2{{conflicting '__sized_by' attribute with the previous function declaration}}
// expected-note@+1{{conflicting attributes were '__counted_by(len)' and '__sized_by(len)'}}
int *__sized_by(len) bayz(unsigned len) {}

// expected-note@+1{{previous declaration is here}}
int *baxz(unsigned len, int **pptr);
// expected-error@+1{{conflicting '__counted_by' attribute with the previous function declaration}}
int *baxz(unsigned len, int *__counted_by(len)* pptr) {} // ok

// expected-note@+1{{previous declaration is here}}
int *baxy(unsigned len, int *__counted_by(len)* pptr);
// expected-error@+1{{conflicting '__counted_by' attribute with the previous function declaration}}
int *baxy(unsigned len, int ** pptr) {}

char *__counted_by(len) qux(int len);
// expected-note@+1{{previous declaration is here}}
char *__sized_by(len) qux(int len);
// expected-error@+2{{conflicting '__counted_by' attribute with the previous function declaration}}
// expected-note@+1{{conflicting attributes were '__sized_by(len)' and '__counted_by(4)'}}
char *__counted_by(4) qux(int len) {}

int *__counted_by(len) __counted_by(len) quux(int len) {} // ok - same count expr

int *__counted_by(len) __counted_by(cnt) quuz(int len, int cnt) {} // expected-error{{pointer cannot have more than one count attribute}}
                                                                   // expected-note@-1{{conflicting attributes were '__counted_by(len)' and '__counted_by(cnt)'}}
                                                                   // expected-note@-2{{previous attribute is here}}

void corge(int *__counted_by(len) ptr, int len);
void corge(int *__counted_by(len) ptr, int len); // ok
void corge(int *__counted_by(len) ptr, int len) {} // ok

// expected-note@+1{{previous declaration is here}}
int ** grault(int len);
// expected-error@+1{{conflicting '__counted_by' attribute with the previous function declaration}}
int *__counted_by(len) grault(int len) {}

int ** grault2(int len);
// expected-error@+1{{'__counted_by' attribute on nested pointer type is only allowed on indirect parameters}}
int *__counted_by(len)* grault2(int len) {}

struct S {
    int *__counted_by(l) // expected-error{{pointer cannot have more than one count attribute}}
                         // c-note@-1{{conflicting attributes were '__counted_by(l + 1)' and '__counted_by(l)'}}
                         // cxx-note@-2{{conflicting attributes were '__counted_by(l + 1)' and '__counted_by(this->l)'}} rdar://139834323 (Struct field in count expression is printed inconsistently in C++ mode)
    bp2
    __counted_by(l+1); // c-note{{previous attribute is here}}
                       // cxx-note@+1{{previous attribute is here}} rdar://139834115 (Count expression has incorrect location in C++ mode)
    int l;
};

struct T {
    unsigned n;
    int * end;
    int *__counted_by(n) __ended_by(end) ptr; // expected-error{{pointer cannot have count and range at the same time}}
    int *__ended_by(end) __counted_by(n) ptr2; // expected-error{{pointer cannot have count and range at the same time}}
};

void end1(int *__ended_by(b) a, int *b); // OK
void end1(int *__ended_by(b) a, int *b); // OK redeclaration

// expected-note@+1 3{{previous declaration is here}}
void end1(int *__ended_by(b) a, int *b) {} // OK definition over declaration

void end1(int *a, int *b); // expected-error{{conflicting '__ended_by' attribute with the previous function declaration}}
void end1(int *a, int *__ended_by(a) b); // expected-error 2{{conflicting '__ended_by' attribute with the previous function declaration}} mismatch on both a and b
void end1(int *__ended_by(b) a, int *__ended_by(a) b); // expected-error{{conflicting '__ended_by' attribute with the previous function declaration}}

void end2(int *__ended_by(b) a, int *__ended_by(c) b, int *c); // OK
void end2(int *__ended_by(b) a, int *__ended_by(c) b, int *c); // OK

// expected-note@+1{{previous declaration is here}}
void end2(int *__ended_by(b) a, int *__ended_by(c) b, int *c) {} // OK definition over declaration

void end2(int *__ended_by(b) a, int *__ended_by(c) b, int *__ended_by(a) c); // expected-error{{conflicting '__ended_by' attribute with the previous function declaration}}

#ifdef __started_by
// this is not currently compiled; it's meant to break if/when __started_by
// explicitly becomes a thing
void start(int *a, int *__started_by(a) b); // OK
void start(int *a, int *__started_by(a) b); // OK redeclaration
// expected-note@+1{{previous declaration is here}} expected-note@+1{{previous declaration is here}}
void start(int *a, int *__started_by(a) b); // OK definition over declaration

void start(int *a, int *__started_by(a) b); // expected-error{{conflicting '__started_by' attribute with the previous function declaration}}
void start(int *a, int *__started_by(a) b); // expected-error{{conflicting '__started_by' attribute with the previous function declaration}}
#endif
