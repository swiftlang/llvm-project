// RUN: %clang_cc1 -std=c++11 -fsyntax-only -fexperimental-new-constant-interpreter -Wno-string-plus-int -Wno-pointer-arith -Wno-zero-length-array -Wno-c99-designator -fsyntax-only -fcxx-exceptions -verify -pedantic %s -Wno-comment -Wno-tautological-pointer-compare -Wno-bool-conversion %s -verify

typedef decltype(sizeof(char)) size_t;


template<typename T> constexpr T id(const T &t) { return t; }

template<typename T, size_t N> constexpr T *begin(T (&xs)[N]) { return xs; }
template<typename T, size_t N> constexpr T *end(T (&xs)[N]) { return xs + N; }

template<typename T> constexpr T min(const T &a, const T &b) {
  return a < b ? a : b;
}
template<typename T> constexpr T max(const T &a, const T &b) {
  return a < b ? b : a;
}

namespace FunctionCast {
  // When folding, we allow functions to be cast to different types. Such
  // cast functions cannot be called, even if they're constexpr.
  constexpr int f() { return 1; }
  typedef double (*DoubleFn)();
  typedef int (*IntFn)();
  int a[(int)DoubleFn(f)()]; // expected-error {{variable length array}} expected-warning{{C99 feature}}
  int b[(int)IntFn(f)()];    // ok
}

struct MemberZero {
  constexpr int zero() const { return 0; }
};

namespace TemplateArgumentConversion {
  template<int n> struct IntParam {};

  using IntParam0 = IntParam<0>;
  using IntParam0 = IntParam<id(0)>;
  using IntParam0 = IntParam<MemberZero().zero>; // expected-error {{did you mean to call it with no arguments?}}
}

namespace StaticAssertFoldTest {

int x;
static_assert(++x, "test"); // expected-error {{not an integral constant expression}}
static_assert(false, "test"); // expected-error {{test}}

}

namespace ConstCast {

constexpr int n1 = 0;
constexpr int n2 = const_cast<int&>(n1);
constexpr int *n3 = const_cast<int*>(&n1);
constexpr int n4 = *const_cast<int*>(&n1);
constexpr const int * const *n5 = const_cast<const int* const*>(&n3);
constexpr int **n6 = const_cast<int**>(&n3);
constexpr int n7 = **n5;
constexpr int n8 = **n6;

// const_cast from prvalue to xvalue.
struct A { int n; };
constexpr int n9 = (const_cast<A&&>(A{123})).n;
static_assert(n9 == 123, "");

}

namespace MemberEnum {
  struct WithMemberEnum {
    enum E { A = 42 };
  } wme;

  static_assert(wme.A == 42, "");
}

namespace DefaultArguments {

const int z = int();
constexpr int Sum(int a = 0, const int &b = 0, const int *c = &z, char d = 0) {
  return a + b + *c + d;
}
const int four = 4;
constexpr int eight = 8;
constexpr const int twentyseven = 27;
static_assert(Sum() == 0, "");
static_assert(Sum(1) == 1, "");
static_assert(Sum(1, four) == 5, "");
static_assert(Sum(1, eight, &twentyseven) == 36, "");
static_assert(Sum(1, 2, &four, eight) == 15, "");

}


namespace Ellipsis {

// Note, values passed through an ellipsis can't actually be used.
constexpr int F(int a, ...) { return a; }
static_assert(F(0) == 0, "");
static_assert(F(1, 0) == 1, "");
static_assert(F(2, "test") == 2, "");
static_assert(F(3, &F) == 3, "");
int k = 0; // expected-note {{here}}
static_assert(F(4, k) == 3, ""); // expected-error {{constant expression}} expected-note {{read of non-const variable 'k'}}

}

namespace Recursion {
  constexpr int fib(int n) { return n > 1 ? fib(n-1) + fib(n-2) : n; }
  static_assert(fib(11) == 89, "");

  constexpr int gcd_inner(int a, int b) {
    return b == 0 ? a : gcd_inner(b, a % b);
  }
  constexpr int gcd(int a, int b) {
    return gcd_inner(max(a, b), min(a, b));
  }

  static_assert(gcd(1749237, 5628959) == 7, "");
}

namespace StaticMemberFunction {
  struct S {
    static constexpr int k = 42;
    static constexpr int f(int n) { return n * k + 2; }
  } s;

  constexpr int n = s.f(19);
  static_assert(S::f(19) == 800, "");
  static_assert(s.f(19) == 800, "");
  static_assert(n == 800, "");

  constexpr int (*sf1)(int) = &S::f;
  constexpr int (*sf2)(int) = &s.f;
  constexpr const int *sk = &s.k;

  // Note, out_of_lifetime returns an invalid pointer value, but we don't do
  // anything with it (other than copy it around), so there's no UB there.
  constexpr S *out_of_lifetime(S s) { return &s; } // expected-warning {{address of stack}}
  static_assert(out_of_lifetime({})->k == 42, "");
  static_assert(out_of_lifetime({})->f(3) == 128, "");

  // Similarly, using an inactive union member breaks no rules.
  union U {
    int n;
    S s;
  };
  constexpr U u = {0};
  static_assert(u.s.k == 42, "");
  static_assert(u.s.f(1) == 44, "");

  // And likewise for a past-the-end pointer.
  static_assert((&s)[1].k == 42, "");
  static_assert((&s)[1].f(1) == 44, "");
}

namespace ParameterScopes {

  const int k = 42;
  constexpr const int &ObscureTheTruth(const int &a) { return a; }
  constexpr const int &MaybeReturnJunk(bool b, const int a) { // expected-note 2{{declared here}}
    return ObscureTheTruth(b ? a : k);
  }
  static_assert(MaybeReturnJunk(false, 0) == 42, ""); // ok
  constexpr int a = MaybeReturnJunk(true, 0); // expected-error {{constant expression}} expected-note {{read of variable whose lifetime has ended}}

  constexpr const int MaybeReturnNonstaticRef(bool b, const int a) {
    return ObscureTheTruth(b ? a : k);
  }
  static_assert(MaybeReturnNonstaticRef(false, 0) == 42, ""); // ok
  constexpr int b = MaybeReturnNonstaticRef(true, 0); // ok

  constexpr int InternalReturnJunk(int n) {
    return MaybeReturnJunk(true, n); // expected-note {{read of variable whose lifetime has ended}}
  }
  constexpr int n3 = InternalReturnJunk(0); // expected-error {{must be initialized by a constant expression}} expected-note {{in call to 'InternalReturnJunk(0)'}}

  constexpr int LToR(int &n) { return n; }
  constexpr int GrabCallersArgument(bool which, int a, int b) {
    return LToR(which ? b : a);
  }
  static_assert(GrabCallersArgument(false, 1, 2) == 1, "");
  static_assert(GrabCallersArgument(true, 4, 8) == 8, "");

}

namespace Pointers {

  constexpr int f(int n, const int *a, const int *b, const int *c) {
    return n == 0 ? 0 : *a + f(n-1, b, c, a);
  }

  const int x = 1, y = 10, z = 100;
  static_assert(f(23, &x, &y, &z) == 788, "");

  constexpr int g(int n, int a, int b, int c) {
    return f(n, &a, &b, &c);
  }
  static_assert(g(23, x, y, z) == 788, "");

}



namespace FunctionPointers {

  constexpr int Double(int n) { return 2 * n; }
  constexpr int Triple(int n) { return 3 * n; }
  constexpr int Twice(int (*F)(int), int n) { return F(F(n)); }
  constexpr int Quadruple(int n) { return Twice(Double, n); }
  constexpr auto Select(int n) -> int (*)(int) {
    return n == 2 ? &Double : n == 3 ? &Triple : n == 4 ? &Quadruple : 0;
  }
  constexpr int Apply(int (*F)(int), int n) { return F(n); } // expected-note {{subexpression}}

  static_assert(1 + Apply(Select(4), 5) + Apply(Select(3), 7) == 42, "");

  constexpr int Invalid = Apply(Select(0), 0); // expected-error {{must be initialized by a constant expression}} expected-note {{in call to 'Apply(nullptr, 0)'}}

}


namespace MaterializeTemporary {

constexpr int f(const int &r) { return r; }
constexpr int n = f(1);

constexpr bool same(const int &a, const int &b) { return &a == &b; }
constexpr bool sameTemporary(const int &n) { return same(n, n); }

static_assert(n, "");
static_assert(!same(4, 4), "");
static_assert(same(n, n), "");
static_assert(sameTemporary(9), "");

struct A { int &&r; };
struct B { A &&a1; A &&a2; };

constexpr B b1 { { 1 }, { 2 } }; // expected-note {{temporary created here}}
static_assert(&b1.a1 != &b1.a2, "");
static_assert(&b1.a1.r != &b1.a2.r, ""); // expected-error {{constant expression}} expected-note {{outside the expression that created the temporary}}

constexpr B &&b2 { { 3 }, { 4 } }; // expected-note {{temporary created here}}
static_assert(&b1 != &b2, "");
static_assert(&b1.a1 != &b2.a1, ""); // expected-error {{constant expression}} expected-note {{outside the expression that created the temporary}}

constexpr thread_local B b3 { { 1 }, { 2 } }; // expected-error {{constant expression}} expected-note {{reference to temporary}} expected-note {{here}}
void foo() {
  constexpr static B b1 { { 1 }, { 2 } }; // ok
  constexpr thread_local B b2 { { 1 }, { 2 } }; // expected-error {{constant expression}} expected-note {{reference to temporary}} expected-note {{here}}
  constexpr B b3 { { 1 }, { 2 } }; // expected-error {{constant expression}} expected-note {{reference to temporary}} expected-note {{here}}
}

constexpr B &&b4 = ((1, 2), 3, 4, B { {10}, {{20}} });
static_assert(&b4 != &b2, "");

// Proposed DR: copy-elision doesn't trigger lifetime extension.
constexpr B b5 = B{ {0}, {0} }; // expected-error {{constant expression}} expected-note {{reference to temporary}} expected-note {{here}}

namespace NestedNonStatic {
  // Proposed DR: for a reference constant expression to refer to a static
  // storage duration temporary, that temporary must itself be initialized
  // by a constant expression (a core constant expression is not enough).
  struct A { int &&r; };
  struct B { A &&a; };
  constexpr B a = { A{0} }; // ok
  constexpr B b = { A(A{0}) }; // expected-error {{constant expression}} expected-note {{reference to temporary}} expected-note {{here}}
}

namespace FakeInitList {
  struct init_list_3_ints { const int (&x)[3]; };
  struct init_list_2_init_list_3_ints { const init_list_3_ints (&x)[2]; };
  constexpr init_list_2_init_list_3_ints ils = { { { { 1, 2, 3 } }, { { 4, 5, 6 } } } };
}

namespace ConstAddedByReference {
  const int &r = (0);
  constexpr int n = r;

  struct A { constexpr operator int() const { return 0; }};
  struct B { constexpr operator const int() const { return 0; }};
  const int &ra = A();
  const int &rb = B();
  constexpr int na = ra;
  constexpr int nb = rb;
}

}

constexpr int strcmp_ce(const char *p, const char *q) {
  return (!*p || *p != *q) ? *p - *q : strcmp_ce(p+1, q+1);
}

namespace StringLiteral {

template<typename Char>
constexpr int MangleChars(const Char *p) {
  return *p + 3 * (*p ? MangleChars(p+1) : 0);
}

static_assert(MangleChars("constexpr!") == 1768383, "");
static_assert(MangleChars(u8"constexpr!") == 1768383, "");
static_assert(MangleChars(L"constexpr!") == 1768383, "");
static_assert(MangleChars(u"constexpr!") == 1768383, "");
static_assert(MangleChars(U"constexpr!") == 1768383, "");

constexpr char c0 = "nought index"[0];
constexpr char c1 = "nice index"[10];
constexpr char c2 = "nasty index"[12]; // expected-error {{must be initialized by a constant expression}} expected-note {{read of dereferenced one-past-the-end pointer}}
constexpr char c3 = "negative index"[-1]; // expected-error {{must be initialized by a constant expression}} expected-note {{cannot refer to element -1 of array of 15 elements}}
constexpr char c4 = ((char*)(int*)"no reinterpret_casts allowed")[14]; // expected-error {{must be initialized by a constant expression}} expected-note {{cast that performs the conversions of a reinterpret_cast}}

constexpr const char *p = "test" + 2;
static_assert(*p == 's', "");

constexpr const char *max_iter(const char *a, const char *b) {
  return *a < *b ? b : a;
}
constexpr const char *max_element(const char *a, const char *b) {
  return (a+1 >= b) ? a : max_iter(a, max_element(a+1, b));
}

constexpr char str[] = "the quick brown fox jumped over the lazy dog";
constexpr const char *max = max_element(begin(str), end(str));
static_assert(*max == 'z', "");
static_assert(max == str + 38, "");

static_assert(strcmp_ce("hello world", "hello world") == 0, "");
static_assert(strcmp_ce("hello world", "hello clang") > 0, "");
static_assert(strcmp_ce("constexpr", "test") < 0, "");
static_assert(strcmp_ce("", " ") < 0, "");

struct S {
  int n : "foo"[4]; // expected-error {{constant expression}} expected-note {{read of dereferenced one-past-the-end pointer is not allowed in a constant expression}}
};

struct T {
  char c[6];
  constexpr T() : c{"foo"} {}
};
constexpr T t;

static_assert(t.c[0] == 'f', "");
static_assert(t.c[1] == 'o', "");
static_assert(t.c[2] == 'o', "");
static_assert(t.c[3] == 0, "");
static_assert(t.c[4] == 0, "");
static_assert(t.c[5] == 0, "");
static_assert(t.c[6] == 0, ""); // expected-error {{constant expression}} expected-note {{one-past-the-end}}

struct U {
  wchar_t chars[6];
  int n;
} constexpr u = { { L"test" }, 0 };
static_assert(u.chars[2] == L's', "");

struct V {
  char c[4];
  constexpr V() : c("hi!") {}
};
static_assert(V().c[1] == "i"[0], "");

namespace Parens {
  constexpr unsigned char a[] = ("foo"), b[] = {"foo"}, c[] = {("foo")},
                          d[4] = ("foo"), e[5] = {"foo"}, f[6] = {("foo")};
  static_assert(a[0] == 'f', "");
  static_assert(b[1] == 'o', "");
  static_assert(c[2] == 'o', "");
  static_assert(d[0] == 'f', "");
  static_assert(e[1] == 'o', "");
  static_assert(f[2] == 'o', "");
  static_assert(f[5] == 0, "");
  static_assert(f[6] == 0, ""); // expected-error {{constant expression}} expected-note {{one-past-the-end}}
}

}

namespace Class {

struct A { constexpr A(int a, int b) : k(a + b) {} int k; };
constexpr int fn(const A &a) { return a.k; }
static_assert(fn(A(4,5)) == 9, "");

struct B { int n; int m; } constexpr b = { 0, b.n };
struct C {
  constexpr C(C *this_) : m(42), n(this_->m) {} // ok
  int m, n;
};
struct D {
  C c;
  constexpr D() : c(&c) {}
};
static_assert(D().c.n == 42, "");

struct E {
  constexpr E() : p(&p) {}
  void *p;
};
constexpr const E &e1 = E();
// This is a constant expression if we elide the copy constructor call, and
// is not a constant expression if we don't! But we do, so it is.
constexpr E e2 = E();
static_assert(e2.p == &e2.p, "");
constexpr E e3;
static_assert(e3.p == &e3.p, "");

extern const class F f;
struct F {
  constexpr F() : p(&f.p) {}
  const void *p;
};
constexpr F f;

struct G {
  struct T {
    constexpr T(T *p) : u1(), u2(p) {}
    union U1 {
      constexpr U1() {}
      int a, b = 42;
    } u1;
    union U2 {
      constexpr U2(T *p) : c(p->u1.b) {}
      int c, d;
    } u2;
  } t;
  constexpr G() : t(&t) {}
} constexpr g;

static_assert(g.t.u1.a == 42, ""); // expected-error {{constant expression}} expected-note {{read of member 'a' of union with active member 'b'}}
static_assert(g.t.u1.b == 42, "");
static_assert(g.t.u2.c == 42, "");
static_assert(g.t.u2.d == 42, ""); // expected-error {{constant expression}} expected-note {{read of member 'd' of union with active member 'c'}}

struct S {
  int a, b;
  const S *p;
  double d;
  const char *q;

  constexpr S(int n, const S *p) : a(5), b(n), p(p), d(n), q("hello") {}
};

S global(43, &global);

static_assert(S(15, &global).b == 15, "");

constexpr bool CheckS(const S &s) {
  return s.a == 5 && s.b == 27 && s.p == &global && s.d == 27. && s.q[3] == 'l';
}
static_assert(CheckS(S(27, &global)), "");

struct Arr {
  char arr[3];
  constexpr Arr() : arr{'x', 'y', 'z'} {}
};
constexpr int hash(Arr &&a) {
  return a.arr[0] + a.arr[1] * 0x100 + a.arr[2] * 0x10000;
}
constexpr int k = hash(Arr());
static_assert(k == 0x007a7978, "");


struct AggregateInit {
  const char &c;
  int n;
  double d;
  int arr[5];
  void *p;
};

constexpr AggregateInit agg1 = { "hello"[0] };

static_assert(strcmp_ce(&agg1.c, "hello") == 0, "");
static_assert(agg1.n == 0, "");
static_assert(agg1.d == 0.0, "");
static_assert(agg1.arr[-1] == 0, ""); // expected-error {{constant expression}} expected-note {{cannot refer to element -1}}
static_assert(agg1.arr[0] == 0, "");
static_assert(agg1.arr[4] == 0, "");
static_assert(agg1.arr[5] == 0, ""); // expected-error {{constant expression}} expected-note {{read of dereferenced one-past-the-end}}
static_assert(agg1.p == nullptr, "");

static constexpr const unsigned char uc[] = { "foo" };
static_assert(uc[0] == 'f', "");
static_assert(uc[3] == 0, "");

namespace SimpleDerivedClass {

struct B {
  constexpr B(int n) : a(n) {}
  int a;
};
struct D : B {
  constexpr D(int n) : B(n) {}
};
constexpr D d(3);
static_assert(d.a == 3, "");

}

struct Bottom { constexpr Bottom() {} };
struct Base : Bottom {
  constexpr Base(int a = 42, const char *b = "test") : a(a), b(b) {}
  int a;
  const char *b;
};
struct Base2 : Bottom {
  constexpr Base2(const int &r) : r(r) {}
  int q = 123;
  const int &r;
};
struct Derived : Base, Base2 {
  constexpr Derived() : Base(76), Base2(a) {}
  int c = r + b[1];
};

constexpr bool operator==(const Base &a, const Base &b) {
  return a.a == b.a && strcmp_ce(a.b, b.b) == 0;
}

constexpr Base base;
constexpr Base base2(76);
constexpr Derived derived;
static_assert(derived.a == 76, "");
static_assert(derived.b[2] == 's', "");
static_assert(derived.c == 76 + 'e', "");
static_assert(derived.q == 123, "");
static_assert(derived.r == 76, "");
static_assert(&derived.r == &derived.a, "");

static_assert(!(derived == base), "");
static_assert(derived == base2, "");

constexpr Bottom &bot1 = (Base&)derived;
constexpr Bottom &bot2 = (Base2&)derived;
static_assert(&bot1 != &bot2, "");

constexpr Bottom *pb1 = (Base*)&derived;
constexpr Bottom *pb2 = (Base2*)&derived;
static_assert(&pb1 != &pb2, "");
static_assert(pb1 == &bot1, "");
static_assert(pb2 == &bot2, "");

constexpr Base2 &fail = (Base2&)bot1; // expected-error {{constant expression}} expected-note {{cannot cast object of dynamic type 'const Class::Derived' to type 'Class::Base2'}}
constexpr Base &fail2 = (Base&)*pb2; // expected-error {{constant expression}} expected-note {{cannot cast object of dynamic type 'const Class::Derived' to type 'Class::Base'}}
constexpr Base2 &ok2 = (Base2&)bot2;
static_assert(&ok2 == &derived, "");

constexpr Base2 *pfail = (Base2*)pb1; // expected-error {{constant expression}} expected-note {{cannot cast object of dynamic type 'const Class::Derived' to type 'Class::Base2'}}
constexpr Base *pfail2 = (Base*)&bot2; // expected-error {{constant expression}} expected-note {{cannot cast object of dynamic type 'const Class::Derived' to type 'Class::Base'}}
constexpr Base2 *pok2 = (Base2*)pb2;
static_assert(pok2 == &derived, "");
static_assert(&ok2 == pok2, "");
static_assert((Base2*)(Derived*)(Base*)pb1 == pok2, "");
static_assert((Derived*)(Base*)pb1 == (Derived*)pok2, "");

// Core issue 903: we do not perform constant evaluation when checking for a
// null pointer in C++11. Just check for an integer literal with value 0.
constexpr Base *nullB = 42 - 6 * 7; // expected-error {{cannot initialize a variable of type 'Class::Base *const' with an rvalue of type 'int'}}
constexpr Base *nullB1 = 0;
static_assert((Bottom*)nullB == 0, "");
static_assert((Derived*)nullB == 0, "");
static_assert((void*)(Bottom*)nullB == (void*)(Derived*)nullB, "");
Base *nullB2 = '\0'; // expected-error {{cannot initialize a variable of type 'Class::Base *' with an rvalue of type 'char'}}
Base *nullB3 = (0);
Base *nullB4 = false; // expected-error {{cannot initialize a variable of type 'Class::Base *' with an rvalue of type 'bool'}}
Base *nullB5 = ((0ULL));
Base *nullB6 = 0.; // expected-error {{cannot initialize a variable of type 'Class::Base *' with an rvalue of type 'double'}}
enum Null { kNull };
Base *nullB7 = kNull; // expected-error {{cannot initialize a variable of type 'Class::Base *' with an rvalue of type 'Class::Null'}}
static_assert(nullB1 == (1 - 1), ""); // expected-error {{comparison between pointer and integer}}



namespace ConversionOperators {

struct T {
  constexpr T(int n) : k(5*n - 3) {}
  constexpr operator int() const { return k; }
  int k;
};

struct S {
  constexpr S(int n) : k(2*n + 1) {}
  constexpr operator int() const { return k; }
  constexpr operator T() const { return T(k); }
  int k;
};

constexpr bool check(T a, T b) { return a == b.k; }

static_assert(S(5) == 11, "");
static_assert(check(S(5), 11), "");

namespace PR14171 {

struct X {
  constexpr (operator int)() const { return 0; }
};
static_assert(X() == 0, "");

}

}

struct This {
  constexpr int f() const { return 0; }
  static constexpr int g() { return 0; }
  void h() {
    constexpr int x = f(); // expected-error {{must be initialized by a constant}}
    // expected-note@-1 {{implicit use of 'this' pointer is only allowed within the evaluation of a call to a 'constexpr' member function}}
    constexpr int y = this->f(); // expected-error {{must be initialized by a constant}}
    // expected-note-re@-1 {{{{^}}use of 'this' pointer}}
    constexpr int z = g();
    static_assert(z == 0, "");
  }
};

}
namespace Complex {

class complex {
  int re, im;
public:
  constexpr complex(int re = 0, int im = 0) : re(re), im(im) {}
  constexpr complex(const complex &o) : re(o.re), im(o.im) {}
  constexpr complex operator-() const { return complex(-re, -im); }
  friend constexpr complex operator+(const complex &l, const complex &r) {
    return complex(l.re + r.re, l.im + r.im);
  }
  friend constexpr complex operator-(const complex &l, const complex &r) {
    return l + -r;
  }
  friend constexpr complex operator*(const complex &l, const complex &r) {
    return complex(l.re * r.re - l.im * r.im, l.re * r.im + l.im * r.re);
  }
  friend constexpr bool operator==(const complex &l, const complex &r) {
    return l.re == r.re && l.im == r.im;
  }
  constexpr bool operator!=(const complex &r) const {
    return re != r.re || im != r.im;
  }
  constexpr int real() const { return re; }
  constexpr int imag() const { return im; }
};

constexpr complex i = complex(0, 1);
constexpr complex k = (3 + 4*i) * (6 - 4*i);
static_assert(complex(1,0).real() == 1, "");
static_assert(complex(1,0).imag() == 0, "");
static_assert(((complex)1).imag() == 0, "");
static_assert(k.real() == 34, "");
static_assert(k.imag() == 12, "");
static_assert(k - 34 == 12*i, "");
static_assert((complex)1 == complex(1), "");
static_assert((complex)1 != complex(0, 1), "");
static_assert(complex(1) == complex(1), "");
static_assert(complex(1) != complex(0, 1), "");
constexpr complex makeComplex(int re, int im) { return complex(re, im); }
static_assert(makeComplex(1,0) == complex(1), "");
static_assert(makeComplex(1,0) != complex(0, 1), "");

class complex_wrap : public complex {
public:
  constexpr complex_wrap(int re, int im = 0) : complex(re, im) {}
  constexpr complex_wrap(const complex_wrap &o) : complex(o) {}
};

static_assert((complex_wrap)1 == complex(1), "");
static_assert((complex)1 != complex_wrap(0, 1), "");
static_assert(complex(1) == complex_wrap(1), "");
static_assert(complex_wrap(1) != complex(0, 1), "");
constexpr complex_wrap makeComplexWrap(int re, int im) {
  return complex_wrap(re, im);
}
static_assert(makeComplexWrap(1,0) == complex(1), "");
static_assert(makeComplexWrap(1,0) != complex(0, 1), "");

}

// _Atomic(T) is exactly like T for the purposes of constant expression
// evaluation..
namespace Atomic {
  constexpr _Atomic int n = 3; // expected-warning {{'_Atomic' is a C11 extension}}

  struct S { _Atomic(double) d; }; // expected-warning {{'_Atomic' is a C11 extension}}
  constexpr S s = { 0.5 };
  constexpr double d1 = s.d;
  constexpr double d2 = n;
  constexpr _Atomic double d3 = n; // expected-warning {{'_Atomic' is a C11 extension}}

  constexpr _Atomic(int) n2 = d3; // expected-warning {{'_Atomic' is a C11 extension}}
  static_assert(d1 == 0.5, "");
  static_assert(d3 == 3.0, "");

  namespace PR16056 {
    struct TestVar {
      _Atomic(int) value; // expected-warning {{'_Atomic' is a C11 extension}}
      constexpr TestVar(int value) : value(value) {}
    };
    constexpr TestVar testVar{-1};
    static_assert(testVar.value == -1, "");
  }

  namespace PR32034 {
    struct A {};
    struct B { _Atomic(A) a; }; // expected-warning {{'_Atomic' is a C11 extension}}
    constexpr int n = (B(), B(), 0);

    struct C { constexpr C() {} void *self = this; };
    constexpr _Atomic(C) c = C(); // expected-warning {{'_Atomic' is a C11 extension}}
  }
}

namespace InstantiateCaseStmt {
  template<int x> constexpr int f() { return x; }
  template<int x> int g(int c) { switch(c) { case f<x>(): return 1; } return 0; }
  int gg(int c) { return g<4>(c); }
}

namespace IndirectField {
  struct S {
    struct { // expected-warning {{GNU extension}}
      union { // expected-warning {{declared in an anonymous struct}}
        struct { // expected-warning {{GNU extension}} expected-warning {{declared in an anonymous union}}
          int a;
          int b;
        };
        int c;
      };
      int d;
    };
    union {
      int e;
      int f;
    };
    constexpr S(int a, int b, int d, int e) : a(a), b(b), d(d), e(e) {}
    constexpr S(int c, int d, int f) : c(c), d(d), f(f) {}
  };

  constexpr S s1(1, 2, 3, 4);
  constexpr S s2(5, 6, 7);

  // FIXME: The diagnostics here do a very poor job of explaining which unnamed
  // member is active and which is requested.
  static_assert(s1.a == 1, "");
  static_assert(s1.b == 2, "");
  static_assert(s1.c == 0, ""); // expected-error {{constant expression}} expected-note {{union with active member}}
  static_assert(s1.d == 3, "");
  static_assert(s1.e == 4, "");
  static_assert(s1.f == 0, ""); // expected-error {{constant expression}} expected-note {{union with active member}}

  static_assert(s2.a == 0, ""); // expected-error {{constant expression}} expected-note {{union with active member}}
  static_assert(s2.b == 0, ""); // expected-error {{constant expression}} expected-note {{union with active member}}
  static_assert(s2.c == 5, "");
  static_assert(s2.d == 6, "");
  static_assert(s2.e == 0, ""); // expected-error {{constant expression}} expected-note {{union with active member}}
  static_assert(s2.f == 7, "");
}

namespace ExprWithCleanups {
  struct A { A(); ~A(); int get(); };
  constexpr int get(bool FromA) { return FromA ? A().get() : 1; }
  constexpr int n = get(false);
}

namespace DR1454 {

constexpr const int &f(const int &n) { return n; }
constexpr int k1 = f(0); // ok

struct Wrap {
  const int &value;
};
constexpr const Wrap &g(const Wrap &w) { return w; }
constexpr int k2 = g({0}).value; // ok

// The temporary here has static storage duration, so we can bind a constexpr
// reference to it.
constexpr const int &i = 1;
constexpr const int j = i;
static_assert(j == 1, "");

// The temporary here is not const, so it can't be read outside the expression
// in which it was created (per the C++14 rules, which we use to avoid a C++11
// defect).
constexpr int &&k = 1; // expected-note {{temporary created here}}
constexpr const int l = k; // expected-error {{constant expression}} expected-note {{read of temporary}}

void f() {
  // The temporary here has automatic storage duration, so we can't bind a
  // constexpr reference to it.
  constexpr const int &i = 1; // expected-error {{constant expression}} expected-note 2{{temporary}}
}

}


namespace RecursiveOpaqueExpr {
  template<typename Iter>
  constexpr auto LastNonzero(Iter p, Iter q) -> decltype(+*p) {
    return p != q ? (LastNonzero(p+1, q) ?: *p) : 0; // expected-warning {{GNU}}
  }

  constexpr int arr1[] = { 1, 0, 0, 3, 0, 2, 0, 4, 0, 0 };
  static_assert(LastNonzero(begin(arr1), end(arr1)) == 4, "");

  constexpr int arr2[] = { 1, 0, 0, 3, 0, 2, 0, 4, 0, 5 };
  static_assert(LastNonzero(begin(arr2), end(arr2)) == 5, "");

  constexpr int arr3[] = {
    1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0,
    1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0,
    1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0,
    1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0,
    1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0,
    1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0,
    1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0,
    2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  static_assert(LastNonzero(begin(arr3), end(arr3)) == 2, "");
}

namespace VLASizeof {

  void f(int k) {
    int arr[k]; // expected-warning {{C99}}
    constexpr int n = 1 +
        sizeof(arr) // expected-error {{constant expression}}
        * 3;
  }
}


// PR12626, redux
namespace InvalidClasses {
  void test0() {
    struct X; // expected-note {{forward declaration}}
    struct Y { bool b; X x; }; // expected-error {{field has incomplete type}}
    Y y;
    auto& b = y.b;
  }
}


namespace NamespaceAlias {
  constexpr int f() {
    namespace NS = NamespaceAlias; // expected-warning {{use of this statement in a constexpr function is a C++14 extension}}
    return &NS::f != nullptr;
  }
}

// Constructors can be implicitly constexpr, even for a non-literal type.
namespace ImplicitConstexpr {
  struct Q { Q() = default; Q(const Q&) = default; Q(Q&&) = default; ~Q(); }; // expected-note 3{{here}}
  struct R { constexpr R() noexcept; constexpr R(const R&) noexcept; constexpr R(R&&) noexcept; ~R() noexcept; };
  struct S { R r; }; // expected-note 3{{here}}
  struct T { T(const T&) noexcept; T(T &&) noexcept; ~T() noexcept; };
  struct U { T t; }; // expected-note 3{{here}}
  static_assert(!__is_literal_type(Q), "");
  static_assert(!__is_literal_type(R), "");
  static_assert(!__is_literal_type(S), "");
  static_assert(!__is_literal_type(T), "");
  static_assert(!__is_literal_type(U), "");
  struct Test {
    friend Q::Q() noexcept; // expected-error {{follows constexpr}}
    friend Q::Q(Q&&) noexcept; // expected-error {{follows constexpr}}
    friend Q::Q(const Q&) noexcept; // expected-error {{follows constexpr}}
    friend S::S() noexcept; // expected-error {{follows constexpr}}
    friend S::S(S&&) noexcept; // expected-error {{follows constexpr}}
    friend S::S(const S&) noexcept; // expected-error {{follows constexpr}}
    friend constexpr U::U() noexcept; // expected-error {{follows non-constexpr}}
    friend constexpr U::U(U&&) noexcept; // expected-error {{follows non-constexpr}}
    friend constexpr U::U(const U&) noexcept; // expected-error {{follows non-constexpr}}
  };
}

// Indirectly test that an implicit lvalue to xvalue conversion performed for
// an NRVO move operation isn't implemented as CK_LValueToRValue.
namespace PR12826 {
  struct Foo {};
  constexpr Foo id(Foo x) { return x; }
  constexpr Foo res(id(Foo()));
}

namespace PR13273 {
  struct U {
    int t;
    U() = default;
  };

  struct S : U {
    S() = default;
  };

  // S's default constructor isn't constexpr, because U's default constructor
  // doesn't initialize 't', but it's trivial, so value-initialization doesn't
  // actually call it.
  static_assert(S{}.t == 0, "");
}

namespace PR12670 {
  struct S {
    constexpr S(int a0) : m(a0) {}
    constexpr S() : m(6) {}
    int m;
  };
  constexpr S x[3] = { {4}, 5 };
  static_assert(x[0].m == 4, "");
  static_assert(x[1].m == 5, "");
  static_assert(x[2].m == 6, "");
}

// Indirectly test that an implicit lvalue-to-rvalue conversion is performed
// when a conditional operator has one argument of type void and where the other
// is a glvalue of class type.
namespace ConditionalLValToRVal {
  struct A {
    constexpr A(int a) : v(a) {}
    int v;
  };

  constexpr A f(const A &a) {
    return a.v == 0 ? throw a : a;
  }

  constexpr A a(4);
  static_assert(f(a).v == 4, "");
}


namespace TLS {
  __thread int n;
  int m;

  constexpr bool b = &n == &n;

  constexpr int *p = &n; // expected-error{{constexpr variable 'p' must be initialized by a constant expression}}

  constexpr int *f() { return &n; }
  constexpr int *q = f(); // expected-error{{constexpr variable 'q' must be initialized by a constant expression}}
  constexpr bool c = f() == f();

  constexpr int *g() { return &m; }
  constexpr int *r = g();
}

namespace Void {
  constexpr void f() { return; } // expected-error{{constexpr function's return type 'void' is not a literal type}}

  void assert_failed(const char *msg, const char *file, int line); // expected-note {{declared here}}
#define ASSERT(expr) ((expr) ? static_cast<void>(0) : assert_failed(#expr, __FILE__, __LINE__))
  template<typename T, size_t S>
  constexpr T get(T (&a)[S], size_t k) {
    return ASSERT(k > 0 && k < S), a[k]; // expected-note{{non-constexpr function 'assert_failed'}}
  }
#undef ASSERT
  template int get(int (&a)[4], size_t);
  constexpr int arr[] = { 4, 1, 2, 3, 4 };
  static_assert(get(arr, 1) == 1, "");
  static_assert(get(arr, 4) == 4, "");
  static_assert(get(arr, 0) == 4, ""); // expected-error{{not an integral constant expression}} \
  // expected-note{{in call to 'get(arr, 0)'}}
}

namespace std { struct type_info; }

namespace TypeId {
  struct A { virtual ~A(); };
  A f();
  A &g();
  constexpr auto &x = typeid(f());
  constexpr auto &y = typeid(g()); // expected-error{{constant expression}} \
  // expected-note{{typeid applied to expression of polymorphic type 'TypeId::A' is not allowed in a constant expression}} \
  // expected-warning {{expression with side effects will be evaluated despite being used as an operand to 'typeid'}}
}


namespace PR14203 {
  struct duration {
    constexpr duration() {}
    constexpr operator int() const { return 0; }
  };
  // These are valid per P0859R0 (moved as DR).
  template<typename T> void f() {
    constexpr duration d = duration();
  }
  int n = sizeof(short{duration(duration())});
}

namespace PR15884 {
  struct S {};
  constexpr S f() { return {}; }
  constexpr S *p = &f();
  // expected-error@-1 {{taking the address of a temporary}}
  // expected-error@-2 {{constexpr variable 'p' must be initialized by a constant expression}}
  // expected-note@-3 {{pointer to temporary is not a constant expression}}
  // expected-note@-4 {{temporary created here}}
}

namespace AfterError {
  // FIXME: Suppress the 'no return statements' diagnostic if the body is invalid.
  constexpr int error() { // expected-error {{no return statement}}
    return foobar; // expected-error {{undeclared identifier}}
  }
  constexpr int k = error();
}


namespace std {
  typedef decltype(sizeof(int)) size_t;

  template <class _E>
  class initializer_list
  {
    const _E* __begin_;
    size_t    __size_;

    constexpr initializer_list(const _E* __b, size_t __s)
      : __begin_(__b),
        __size_(__s)
    {}

  public:
    typedef _E        value_type;
    typedef const _E& reference;
    typedef const _E& const_reference;
    typedef size_t    size_type;

    typedef const _E* iterator;
    typedef const _E* const_iterator;

    constexpr initializer_list() : __begin_(nullptr), __size_(0) {}

    constexpr size_t    size()  const {return __size_;}
    constexpr const _E* begin() const {return __begin_;}
    constexpr const _E* end()   const {return __begin_ + __size_;}
  };
}

namespace InitializerList {
  constexpr int sum(const int *b, const int *e) {
    return b != e ? *b + sum(b+1, e) : 0;
  }
  constexpr int sum(std::initializer_list<int> ints) {
    return sum(ints.begin(), ints.end());
  }
  static_assert(sum({1, 2, 3, 4, 5}) == 15, "");

  static_assert(*std::initializer_list<int>{1, 2, 3}.begin() == 1, "");
  static_assert(std::initializer_list<int>{1, 2, 3}.begin()[2] == 3, "");
}

namespace VirtualFromBase {
  struct S1 {
    virtual int f() const;
  };
  struct S2 {
    virtual int f();
  };
  template <typename T> struct X : T {
    constexpr X() {}
    double d = 0.0;
    constexpr int f() { return sizeof(T); } // expected-warning {{will not be implicitly 'const' in C++14}}
  };

  // Virtual f(), not OK.
  constexpr X<X<S1>> xxs1;
  constexpr X<S1> *p = const_cast<X<X<S1>>*>(&xxs1);
  static_assert(p->f() == sizeof(X<S1>), ""); // expected-error {{constant expression}} expected-note {{virtual function}}

  // Non-virtual f(), OK.
  constexpr X<X<S2>> xxs2;
  constexpr X<S2> *q = const_cast<X<X<S2>>*>(&xxs2);
  static_assert(q->f() == sizeof(S2), "");
}

namespace ConstexprConstructorRecovery {
  class X {
  public:
      enum E : short {
          headers = 0x1,
          middlefile = 0x2,
          choices = 0x4
      };
      constexpr X() noexcept {};
  protected:
      E val{0}; // expected-error {{cannot initialize a member subobject of type 'ConstexprConstructorRecovery::X::E' with an rvalue of type 'int'}} expected-note {{here}}
  };
  // FIXME: We should avoid issuing this follow-on diagnostic.
  constexpr X x{}; // expected-error {{constant expression}} expected-note {{not initialized}}
}



namespace BadDefaultInit {
  template<int N> struct X { static const int n = N; };

  struct A { // expected-error {{default member initializer for 'k' needed within definition of enclosing class}}
    int k = // expected-note {{default member initializer declared here}}
        X<A().k>::n; // expected-note {{in evaluation of exception specification for 'BadDefaultInit::A::A' needed here}}
  };

  // FIXME: The "constexpr constructor must initialize all members" diagnostic
  // here is bogus (we discard the k(k) initializer because the parameter 'k'
  // has been marked invalid).
  struct B { // expected-note 2{{candidate}}
    constexpr B( // expected-warning {{initialize all members}} expected-note {{candidate}}
        int k = X<B().k>::n) : // expected-error {{no matching constructor}}
      k(k) {}
    int k; // expected-note {{not initialized}}
  };
}

namespace PR17800 {
  struct A {
    constexpr int operator()() const { return 0; }
  };
  template <typename ...T> constexpr int sink(T ...) {
    return 0;
  }
  template <int ...N> constexpr int run() {
    return sink(A()() + N ...);
  }
  constexpr int k = run<1, 2, 3>();
}

namespace BuiltinStrlen {
  constexpr const char *a = "foo\0quux";
  constexpr char b[] = "foo\0quux";
  constexpr int f() { return 'u'; }
  constexpr char c[] = { 'f', 'o', 'o', 0, 'q', f(), 'u', 'x', 0 };

  static_assert(__builtin_strlen("foo") == 3, "");
  static_assert(__builtin_strlen("foo\0quux") == 3, "");
  static_assert(__builtin_strlen("foo\0quux" + 4) == 4, "");

  constexpr bool check(const char *p) {
    return __builtin_strlen(p) == 3 &&
           __builtin_strlen(p + 1) == 2 &&
           __builtin_strlen(p + 2) == 1 &&
           __builtin_strlen(p + 3) == 0 &&
           __builtin_strlen(p + 4) == 4 &&
           __builtin_strlen(p + 5) == 3 &&
           __builtin_strlen(p + 6) == 2 &&
           __builtin_strlen(p + 7) == 1 &&
           __builtin_strlen(p + 8) == 0;
  }

  static_assert(check(a), "");
  static_assert(check(b), "");
  static_assert(check(c), "");

  constexpr int over1 = __builtin_strlen(a + 9); // expected-error {{constant expression}} expected-note {{one-past-the-end}}
  constexpr int over2 = __builtin_strlen(b + 9); // expected-error {{constant expression}} expected-note {{one-past-the-end}}
  constexpr int over3 = __builtin_strlen(c + 9); // expected-error {{constant expression}} expected-note {{one-past-the-end}}

  constexpr int under1 = __builtin_strlen(a - 1); // expected-error {{constant expression}} expected-note {{cannot refer to element -1}}
  constexpr int under2 = __builtin_strlen(b - 1); // expected-error {{constant expression}} expected-note {{cannot refer to element -1}}
  constexpr int under3 = __builtin_strlen(c - 1); // expected-error {{constant expression}} expected-note {{cannot refer to element -1}}

  // FIXME: The diagnostic here could be better.
  constexpr char d[] = { 'f', 'o', 'o' }; // no nul terminator.
  constexpr int bad = __builtin_strlen(d); // expected-error {{constant expression}} expected-note {{one-past-the-end}}
}

namespace PR19010 {
  struct Empty {};
  struct Empty2 : Empty {};
  struct Test : Empty2 {
    constexpr Test() {}
    Empty2 array[2];
  };
  void test() { constexpr Test t; }
}

void PR21327(int a, int b) {
  static_assert(&a + 1 != &b, ""); // expected-error {{constant expression}}
}

namespace EmptyClass {
  struct E1 {} e1;
  union E2 {} e2; // expected-note {{here}}
  struct E3 : E1 {} e3;

  // The defaulted copy constructor for an empty class does not read any
  // members. The defaulted copy constructor for an empty union reads the
  // object representation.
  constexpr E1 e1b(e1);
  constexpr E2 e2b(e2); // expected-error {{constant expression}} expected-note{{read of non-const}} expected-note {{in call}}
  constexpr E3 e3b(e3);
}

namespace PR21859 {
  constexpr int Fun() { return; } // expected-error {{non-void constexpr function 'Fun' should return a value}}
  constexpr int Var = Fun();
}

struct InvalidRedef {
  int f; // expected-note{{previous definition is here}}
  constexpr int f(void); // expected-error{{redefinition of 'f'}} expected-warning{{will not be implicitly 'const'}}
};

namespace PR17938 {
  template <typename T> constexpr T const &f(T const &x) { return x; }

  struct X {};
  struct Y : X {};
  struct Z : Y { constexpr Z() {} };

  static constexpr auto z = f(Z());
}

namespace PR24597 {
  struct A {
    int x, *p;
    constexpr A() : x(0), p(&x) {}
    constexpr A(const A &a) : x(a.x), p(&x) {}
  };
  constexpr A f() { return A(); }
  constexpr A g() { return f(); }
  constexpr int a = *f().p;
  constexpr int b = *g().p;
}

namespace InheritedCtor {
  struct A { constexpr A(int) {} };

  struct B : A { int n; using A::A; }; // expected-note {{here}}
  constexpr B b(0); // expected-error {{constant expression}} expected-note {{derived class}}

  struct C : A { using A::A; struct { union { int n, m = 0; }; union { int a = 0; }; int k = 0; }; struct {}; union {}; }; // expected-warning 6{{}}
  constexpr C c(0);

  struct D : A {
    using A::A; // expected-note {{here}}
    struct { // expected-warning {{extension}}
      union { // expected-warning {{extension}}
        int n;
      };
    };
  };
  constexpr D d(0); // expected-error {{constant expression}} expected-note {{derived class}}

  struct E : virtual A { using A::A; }; // expected-note {{here}}
  // We wrap a function around this to avoid implicit zero-initialization
  // happening first; the zero-initialization step would produce the same
  // error and defeat the point of this test.
  void f() {
    constexpr E e(0); // expected-error {{constant expression}} expected-note {{derived class}}
  }
  // FIXME: This produces a note with no source location.
  //constexpr E e(0);

  struct W { constexpr W(int n) : w(n) {} int w; };
  struct X : W { using W::W; int x = 2; };
  struct Y : X { using X::X; int y = 3; };
  struct Z : Y { using Y::Y; int z = 4; };
  constexpr Z z(1);
  static_assert(z.w == 1 && z.x == 2 && z.y == 3 && z.z == 4, "");
}


namespace PR28366 {
namespace ns1 {

void f(char c) { //expected-note2{{declared here}}
  struct X {
    static constexpr char f() { //expected-error{{never produces a constant expression}}
      return c; //expected-error{{reference to local}} expected-note{{non-const variable}}
    }
  };
  int I = X::f();
}

void g() {
  const int c = 'c';
  static const int d = 'd';
  struct X {
    static constexpr int f() {
      return c + d;
    }
  };
  static_assert(X::f() == 'c' + 'd',"");
}


} // end ns1

} //end ns PR28366


namespace PointerArithmeticOverflow {
  int n;
  int a[1];
  constexpr int *b = &n + 1 + (long)-1;
  constexpr int *c = &n + 1 + (unsigned long)-1; // expected-error {{constant expression}} expected-note {{cannot refer to element 1844}}
  constexpr int *d = &n + 1 - (unsigned long)1;
  constexpr int *e = a + 1 + (long)-1;
  constexpr int *f = a + 1 + (unsigned long)-1; // expected-error {{constant expression}} expected-note {{cannot refer to element 1844}}
  constexpr int *g = a + 1 - (unsigned long)1;

  constexpr int *p = (&n + 1) + (unsigned __int128)-1; // expected-error {{constant expression}} expected-note {{cannot refer to element 3402}}
  constexpr int *q = (&n + 1) - (unsigned __int128)-1; // expected-error {{constant expression}} expected-note {{cannot refer to element -3402}}
  constexpr int *r = &(&n + 1)[(unsigned __int128)-1]; // expected-error {{constant expression}} expected-note {{cannot refer to element 3402}}
}

namespace PR40430 {
  struct S {
    char c[10] = "asdf";
    constexpr char foo() const { return c[3]; }
  };
  static_assert(S().foo() == 'f', "");
}

namespace PR41854 {
  struct e { operator int(); };
  struct f { e c; };
  int a;
  f &d = reinterpret_cast<f&>(a);
  unsigned b = d.c;
}


namespace MemberPointer {
  struct A {
    constexpr A(int n) : n(n) {}
    int n;
    constexpr int f() const { return n + 3; }
  };
  constexpr A a(7);
  static_assert(A(5).*&A::n == 5, "");
  static_assert((&a)->*&A::n == 7, "");
  static_assert((A(8).*&A::f)() == 11, "");
  static_assert(((&a)->*&A::f)() == 10, "");

  struct B : A {
    constexpr B(int n, int m) : A(n), m(m) {}
    int m;
    constexpr int g() const { return n + m + 1; }
  };
  constexpr B b(9, 13);
  static_assert(B(4, 11).*&A::n == 4, "");
  static_assert(B(4, 11).*&B::m == 11, "");
  static_assert(B(4, 11).*(int(A::*))&B::m == 11, "");
  static_assert((&b)->*&A::n == 9, "");
  static_assert((&b)->*&B::m == 13, "");
  static_assert((&b)->*(int(A::*))&B::m == 13, "");
  static_assert((B(4, 11).*&A::f)() == 7, "");
  static_assert((B(4, 11).*&B::g)() == 16, "");
  static_assert((B(4, 11).*(int(A::*)()const)&B::g)() == 16, "");
  static_assert(((&b)->*&A::f)() == 12, "");
  static_assert(((&b)->*&B::g)() == 23, "");
  static_assert(((&b)->*(int(A::*)()const)&B::g)() == 23, "");

  struct S {
    constexpr S(int m, int n, int (S::*pf)() const, int S::*pn) :
      m(m), n(n), pf(pf), pn(pn) {}
    constexpr S() : m(), n(), pf(&S::f), pn(&S::n) {}

    constexpr int f() const { return this->*pn; }
    virtual int g() const;

    int m, n;
    int (S::*pf)() const;
    int S::*pn;
  };

  constexpr int S::*pm = &S::m;
  constexpr int S::*pn = &S::n;
  constexpr int (S::*pf)() const = &S::f;
  constexpr int (S::*pg)() const = &S::g;

  constexpr S s(2, 5, &S::f, &S::m);

  static_assert((s.*&S::f)() == 2, "");
  static_assert((s.*s.pf)() == 2, "");

  static_assert(pf == &S::f, "");
  static_assert(pf == s.*&S::pf, "");
  static_assert(pm == &S::m, "");
  static_assert(pm != pn, "");
  static_assert(s.pn != pn, "");
  static_assert(s.pn == pm, "");
  static_assert(pg != nullptr, "");
  static_assert(pf != nullptr, "");
  static_assert((int S::*)nullptr == nullptr, "");
  static_assert(pg == pg, ""); // expected-error {{constant expression}} expected-note {{comparison of pointer to virtual member function 'g' has unspecified value}}
  static_assert(pf != pg, ""); // expected-error {{constant expression}} expected-note {{comparison of pointer to virtual member function 'g' has unspecified value}}

  template<int n> struct T : T<n-1> {};
  template<> struct T<0> { int n; };
  template<> struct T<30> : T<29> { int m; };

  T<17> t17;
  T<30> t30;

  constexpr int (T<10>::*deepn) = &T<0>::n;
  static_assert(&(t17.*deepn) == &t17.n, "");
  static_assert(deepn == &T<2>::n, "");

  constexpr int (T<15>::*deepm) = (int(T<10>::*))&T<30>::m;
  constexpr int *pbad = &(t17.*deepm); // expected-error {{constant expression}}
  static_assert(&(t30.*deepm) == &t30.m, "");
  static_assert(deepm == &T<50>::m, "");
  static_assert(deepm != deepn, "");

  constexpr T<5> *p17_5 = &t17;
  constexpr T<13> *p17_13 = (T<13>*)p17_5;
  constexpr T<23> *p17_23 = (T<23>*)p17_13; // expected-error {{constant expression}} expected-note {{cannot cast object of dynamic type 'T<17>' to type 'T<23>'}}
  static_assert(&(p17_5->*(int(T<3>::*))deepn) == &t17.n, "");
  static_assert(&(p17_13->*deepn) == &t17.n, "");
  constexpr int *pbad2 = &(p17_13->*(int(T<9>::*))deepm); // expected-error {{constant expression}}

  constexpr T<5> *p30_5 = &t30;
  constexpr T<23> *p30_23 = (T<23>*)p30_5;
  constexpr T<13> *p30_13 = p30_23;
  static_assert(&(p30_5->*(int(T<3>::*))deepn) == &t30.n, "");
  static_assert(&(p30_13->*deepn) == &t30.n, "");
  static_assert(&(p30_23->*deepn) == &t30.n, "");
  static_assert(&(p30_5->*(int(T<2>::*))deepm) == &t30.m, "");
  static_assert(&(((T<17>*)p30_13)->*deepm) == &t30.m, "");
  static_assert(&(p30_23->*deepm) == &t30.m, "");

  struct Base { int n; };
  template<int N> struct Mid : Base {};
  struct Derived : Mid<0>, Mid<1> {};
  static_assert(&Mid<0>::n == &Mid<1>::n, "");
  static_assert((int Derived::*)(int Mid<0>::*)&Mid<0>::n !=
                (int Derived::*)(int Mid<1>::*)&Mid<1>::n, "");
  static_assert(&Mid<0>::n == (int Mid<0>::*)&Base::n, "");

  constexpr int apply(const A &a, int (A::*f)() const) {
    return (a.*f)();
  }
  static_assert(apply(A(2), &A::f) == 5, "");
}


namespace DerivedToVBaseCast {

  struct U { int n; };
  struct V : U { int n; };
  struct A : virtual V { int n; };
  struct Aa { int n; };
  struct B : virtual A, Aa {};
  struct C : virtual A, Aa {};
  struct D : B, C {};

  D d;
  constexpr B *p = &d;
  constexpr C *q = &d;

  static_assert((void*)p != (void*)q, "");
  static_assert((A*)p == (A*)q, "");
  static_assert((Aa*)p != (Aa*)q, "");

  constexpr B &pp = d;
  constexpr C &qq = d;
  static_assert((void*)&pp != (void*)&qq, "");
  static_assert(&(A&)pp == &(A&)qq, "");
  static_assert(&(Aa&)pp != &(Aa&)qq, "");

  constexpr V *v = p;
  constexpr V *w = q;
  constexpr V *x = (A*)p;
  static_assert(v == w, "");
  static_assert(v == x, "");

  static_assert((U*)&d == p, "");
  static_assert((U*)&d == q, "");
  static_assert((U*)&d == v, "");
  static_assert((U*)&d == w, "");
  static_assert((U*)&d == x, "");
}

namespace ArrayEltInit {
  struct A {
    constexpr A() : p(&p) {}
    void *p;
  };
  constexpr A a[10];
  static_assert(a[0].p == &a[0].p, "");
  static_assert(a[9].p == &a[9].p, "");
  static_assert(a[0].p != &a[9].p, "");
  static_assert(a[9].p != &a[0].p, "");

  constexpr A b[10] = {};
  static_assert(b[0].p == &b[0].p, "");
  static_assert(b[9].p == &b[9].p, "");
  static_assert(b[0].p != &b[9].p, "");
  static_assert(b[9].p != &b[0].p, "");
}

namespace Bitfields {
  struct A {
    bool b : 1;
    unsigned u : 5;
    int n : 5;
    bool b2 : 3;
    unsigned u2 : 74; // expected-warning {{exceeds the width of its type}}
    int n2 : 81; // expected-warning {{exceeds the width of its type}}
  };

  constexpr A a = { false, 33, 31, false, 0xffffffff, 0x7fffffff }; // expected-warning 2{{truncation}}
  static_assert(a.b == 0 && a.u == 1 && a.n == -1 && a.b2 == 0 &&
                a.u2 + 1 == 0 && a.n2 == 0x7fffffff,
                "bad truncation of bitfield values");

  struct B {
    int n : 3;
    constexpr B(int k) : n(k) {}
  };
  static_assert(B(3).n == 3, "");
  static_assert(B(4).n == -4, "");
  static_assert(B(7).n == -1, "");
  static_assert(B(8).n == 0, "");
  static_assert(B(-1).n == -1, "");
  static_assert(B(-8889).n == -1, "");

  namespace PR16755 {
    struct X {
      int x : 1;
      constexpr static int f(int x) {
        return X{x}.x;
      }
    };
    static_assert(X::f(3) == -1, "3 should truncate to -1");
  }

  struct HasUnnamedBitfield {
    unsigned a;
    unsigned : 20;
    unsigned b;

    constexpr HasUnnamedBitfield() : a(), b() {}
    constexpr HasUnnamedBitfield(unsigned a, unsigned b) : a(a), b(b) {}
  };

  void testUnnamedBitfield() {
    const HasUnnamedBitfield zero{};
    int a = 1 / zero.b; // expected-warning {{division by zero is undefined}}
    const HasUnnamedBitfield oneZero{1, 0};
    int b = 1 / oneZero.b; // expected-warning {{division by zero is undefined}}
  }
}
