// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify

struct Base { int a; };
struct Derived : public Base { int b; };

Base base;
constexpr Base *ptrBase = &base + 1;
// expected-note@+2 {{cannot access field of pointer past the end of object}}
// expected-error@+1 {{constexpr variable}}
constexpr void *ptrBaseA = &ptrBase->a;

Derived derived;
constexpr Derived *ptrDerived = &derived + 1;
// expected-note@+2 {{cannot access field of pointer past the end of object}}
// expected-error@+1 {{constexpr variable}}
constexpr void *ptrDerivedA = &ptrBase->a;
// expected-note@+2 {{cannot access field of pointer past the end of object}}
// expected-error@+1 {{constexpr variable}}
constexpr void *ptrDerivedB = &ptrBase->a;

constexpr int Array[10][10] = {};
// expected-note@+2 {{cannot access array element of pointer past the end of object}}
// expected-error@+1 {{constexpr variable}}
constexpr const int *decayedElement = Array[10];

constexpr Base arr[2] = {};
// expected-note@+2 {{cannot access field of pointer past the end of object}}
// expected-error@+1 {{constexpr variable}}
constexpr const int *x = &arr[2].a;
