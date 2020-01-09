// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

constexpr int x[10] = {};

constexpr const void *p0 = nullptr;
constexpr const void *p1 = &x[0];
constexpr const void *p2 = &x[1];

static_assert(!(p0 == p1));
static_assert(!(p0 == p2));
static_assert(!(p1 == p2));
static_assert(!(p0 != p0));
static_assert(!(p1 != p1));
static_assert(!(p2 != p2));
static_assert(p0 != p1);
static_assert(p0 != p2);
static_assert(p1 != p2);
static_assert(p0 == p0);
static_assert(p1 == p1);
static_assert(p2 == p2);

constexpr int f() { return 0; }
constexpr int g() { return 0; }


constexpr void *f0 = nullptr;
constexpr void *f1 = (void *)f;
constexpr void *f2 = (void *)g;

static_assert(!(f0 == f1));
static_assert(!(f0 == f2));
static_assert(!(f1 == f2));
static_assert(!(f0 != f0));
static_assert(!(f1 != f1));
static_assert(!(f2 != f2));
static_assert(f0 != f1);
static_assert(f0 != f2);
static_assert(f1 != f2);
static_assert(f0 == f0);
static_assert(f1 == f1);
static_assert(f2 == f2);

static_assert(!(f1 == p1));
static_assert(!(f1 == p1));
static_assert(!(f2 == p1));
static_assert(!(f2 == p1));

static_assert(f1 != p1);
static_assert(f1 != p1);
static_assert(f2 != p1);
static_assert(f2 != p1);
