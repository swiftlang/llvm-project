// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// XFAIL: *

extern int &Recurse1;
int &Recurse2 = Recurse1; // expected-note {{declared here}}
int &Recurse1 = Recurse2;
constexpr int &Recurse3 = Recurse2; // expected-error {{must be initialized by a constant expression}} expected-note {{initializer of 'Recurse2' is not a constant expression}}

extern const int RecurseA;
const int RecurseB = RecurseA; // expected-note {{declared here}}
const int RecurseA = 10;
constexpr int RecurseC = RecurseB; // expected-error {{must be initialized by a constant expression}} expected-note {{initializer of 'RecurseB' is not a constant expression}}


namespace PointerComparison {

int x, y;
static_assert(&x == &y, "false"); // expected-error {{false}}
static_assert(&x != &y, "");
constexpr bool g1 = &x == &y;
constexpr bool g2 = &x != &y;
constexpr bool g3 = &x <= &y; // expected-error {{must be initialized by a constant expression}} expected-note {{unspecified}}
constexpr bool g4 = &x >= &y; // expected-error {{must be initialized by a constant expression}} expected-note {{unspecified}}
constexpr bool g5 = &x < &y; // expected-error {{must be initialized by a constant expression}} expected-note {{unspecified}}
constexpr bool g6 = &x > &y; // expected-error {{must be initialized by a constant expression}} expected-note {{unspecified}}

struct S { int x, y; } s;
static_assert(&s.x == &s.y, "false"); // expected-error {{false}}
static_assert(&s.x != &s.y, "");
static_assert(&s.x <= &s.y, "");
static_assert(&s.x >= &s.y, "false"); // expected-error {{false}}
static_assert(&s.x < &s.y, "");
static_assert(&s.x > &s.y, "false"); // expected-error {{false}}

static_assert(0 == &y, "false"); // expected-error {{false}}
static_assert(0 != &y, "");
constexpr bool n3 = (int*)0 <= &y; // expected-error {{must be initialized by a constant expression}} expected-note {{unspecified}}
constexpr bool n4 = (int*)0 >= &y; // expected-error {{must be initialized by a constant expression}} expected-note {{unspecified}}
constexpr bool n5 = (int*)0 < &y; // expected-error {{must be initialized by a constant expression}} expected-note {{unspecified}}
constexpr bool n6 = (int*)0 > &y; // expected-error {{must be initialized by a constant expression}} expected-note {{unspecified}}

static_assert(&x == 0, "false"); // expected-error {{false}}
static_assert(&x != 0, "");
constexpr bool n9 = &x <= (int*)0; // expected-error {{must be initialized by a constant expression}} expected-note {{unspecified}}
constexpr bool n10 = &x >= (int*)0; // expected-error {{must be initialized by a constant expression}} expected-note {{unspecified}}
constexpr bool n11 = &x < (int*)0; // expected-error {{must be initialized by a constant expression}} expected-note {{unspecified}}
constexpr bool n12 = &x > (int*)0; // expected-error {{must be initialized by a constant expression}} expected-note {{unspecified}}

static_assert(&x == &x, "");
static_assert(&x != &x, "false"); // expected-error {{false}}
static_assert(&x <= &x, "");
static_assert(&x >= &x, "");
static_assert(&x < &x, "false"); // expected-error {{false}}
static_assert(&x > &x, "false"); // expected-error {{false}}

constexpr S* sptr = &s;
constexpr bool dyncast = sptr == dynamic_cast<S*>(sptr); // expected-error {{constant expression}} expected-note {{dynamic_cast}}

struct U {};
struct Str {
  int a : dynamic_cast<S*>(sptr) == dynamic_cast<S*>(sptr); // \
    expected-warning {{not an integral constant expression}} \
    expected-note {{dynamic_cast is not allowed in a constant expression}}
  int b : reinterpret_cast<S*>(sptr) == reinterpret_cast<S*>(sptr); // \
    expected-warning {{not an integral constant expression}} \
    expected-note {{reinterpret_cast is not allowed in a constant expression}}
  int c : (S*)(long)(sptr) == (S*)(long)(sptr); // \
    expected-warning {{not an integral constant expression}} \
    expected-note {{cast that performs the conversions of a reinterpret_cast is not allowed in a constant expression}}
  int d : (S*)(42) == (S*)(42); // \
    expected-warning {{not an integral constant expression}} \
    expected-note {{cast that performs the conversions of a reinterpret_cast is not allowed in a constant expression}}
  int e : (Str*)(sptr) == (Str*)(sptr); // \
    expected-warning {{not an integral constant expression}} \
    expected-note {{cast that performs the conversions of a reinterpret_cast is not allowed in a constant expression}}
  int f : &(U&)(*sptr) == &(U&)(*sptr); // \
    expected-warning {{not an integral constant expression}} \
    expected-note {{cast that performs the conversions of a reinterpret_cast is not allowed in a constant expression}}
  int g : (S*)(void*)(sptr) == sptr; // \
    expected-warning {{not an integral constant expression}} \
    expected-note {{cast from 'void *' is not allowed in a constant expression}}
};

extern char externalvar[];
constexpr bool constaddress = (void *)externalvar == (void *)0x4000UL; // expected-error {{must be initialized by a constant expression}} expected-note {{reinterpret_cast}}
constexpr bool litaddress = "foo" == "foo"; // expected-error {{must be initialized by a constant expression}}
static_assert(0 != "foo", "");

}



// Per current CWG direction, we reject any cases where pointer arithmetic is
// not statically known to be valid.
namespace ArrayOfUnknownBound {
  extern int arr[];
  constexpr int *a = arr;
  constexpr int *b = &arr[0];
  static_assert(a == b, "");
  constexpr int *c = &arr[1]; // expected-error {{constant}} expected-note {{indexing of array without known bound}}
  constexpr int *d = &a[1]; // expected-error {{constant}} expected-note {{indexing of array without known bound}}
  constexpr int *e = a + 1; // expected-error {{constant}} expected-note {{indexing of array without known bound}}

  struct X {
    int a;
    int b[]; // expected-warning {{C99}}
  };
  extern X x;
  constexpr int *xb = x.b; // expected-error {{constant}} expected-note {{not supported}}

  struct Y { int a; };
  extern Y yarr[];
  constexpr Y *p = yarr;
  constexpr int *q = &p->a;

  extern const int carr[]; // expected-note {{here}}
  constexpr int n = carr[0]; // expected-error {{constant}} expected-note {{non-constexpr variable}}

  constexpr int local_extern[] = {1, 2, 3};
  void f() { extern const int local_extern[]; }
  static_assert(local_extern[1] == 2, "");
}

namespace DependentValues {

struct I { int n; typedef I V[10]; };
I::V x, y;
int g(); // expected-note {{declared here}}
template<bool B, typename T> struct S : T {
  int k;
  void f() {
    I::V &cells = B ? x : y;
    I &i = cells[k];
    switch (i.n) {}

    constexpr int n = g(); // expected-error {{must be initialized by a constant expression}} expected-note {{non-constexpr function 'g'}}

    constexpr int m = this->g(); // ok, could be constexpr
  }
};

extern const int n;
template<typename T> void f() {
  // This is ill-formed, because a hypothetical instantiation at the point of
  // template definition would be ill-formed due to a construct that does not
  // depend on a template parameter.
  constexpr int k = n; // expected-error {{must be initialized by a constant expression}}
}
// It doesn't matter that the instantiation could later become valid:
constexpr int n = 4;
template void f<int>();

}

namespace Temporaries {

struct S {
  constexpr S() {}
  constexpr int f() const;
  constexpr int g() const;
};
struct T : S {
  constexpr T(int n) : S(), n(n) {}
  int n;
};
constexpr int S::f() const {
  return static_cast<const T*>(this)->n; // expected-note {{cannot cast}}
}
constexpr int S::g() const {
  // FIXME: Better diagnostic for this.
  return this->*(int(S::*))&T::n; // expected-note {{subexpression}}
}
// The T temporary is implicitly cast to an S subobject, but we can recover the
// T full-object via a base-to-derived cast, or a derived-to-base-casted member
// pointer.
static_assert(S().f(), ""); // expected-error {{constant expression}} expected-note {{in call to '&Temporaries::S()->f()'}}
static_assert(S().g(), ""); // expected-error {{constant expression}} expected-note {{in call to '&Temporaries::S()->g()'}}
static_assert(T(3).f() == 3, "");
static_assert(T(4).g() == 4, "");

constexpr int f(const S &s) {
  return static_cast<const T&>(s).n;
}
constexpr int n = f(T(5));
static_assert(f(T(5)) == 5, "");

constexpr bool b(int n) { return &n; }
static_assert(b(0), "");

struct NonLiteral {
  NonLiteral();
  int f();
};
constexpr int k = NonLiteral().f(); // expected-error {{constant expression}} expected-note {{non-literal type 'Temporaries::NonLiteral'}}

}


namespace Union {

union U {
  int a;
  int b;
};

constexpr U u[4] = { { .a = 0 }, { .b = 1 }, { .a = 2 }, { .b = 3 } };
static_assert(u[0].a == 0, "");
static_assert(u[0].b, ""); // expected-error {{constant expression}} expected-note {{read of member 'b' of union with active member 'a'}}
static_assert(u[1].b == 1, "");
static_assert((&u[1].b)[1] == 2, ""); // expected-error {{constant expression}} expected-note {{read of dereferenced one-past-the-end pointer}}
static_assert(*(&(u[1].b) + 1 + 1) == 3, ""); // expected-error {{constant expression}} expected-note {{cannot refer to element 2 of non-array object}}
static_assert((&(u[1]) + 1 + 1)->b == 3, "");

constexpr U v = {};
static_assert(v.a == 0, "");

union Empty {};
constexpr Empty e = {};

// Make sure we handle trivial copy constructors for unions.
constexpr U x = {42};
constexpr U y = x;
static_assert(y.a == 42, "");
static_assert(y.b == 42, ""); // expected-error {{constant expression}} expected-note {{'b' of union with active member 'a'}}

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

namespace PR11595 {
  struct A { constexpr bool operator==(int x) const { return true; } };
  struct B { B(); A& x; };
  static_assert(B().x == 3, "");  // expected-error {{constant expression}} expected-note {{non-literal type 'PR11595::B' cannot be used in a constant expression}}

  constexpr bool f(int k) { // expected-error {{constexpr function never produces a constant expression}}
    return B().x == k; // expected-note {{non-literal type 'PR11595::B' cannot be used in a constant expression}}
  }
}

namespace Volatile {

volatile constexpr int n1 = 0; // expected-note {{here}}
volatile const int n2 = 0; // expected-note {{here}}
int n3 = 37; // expected-note {{declared here}}

constexpr int m1 = n1; // expected-error {{constant expression}} expected-note {{read of volatile-qualified type 'const volatile int'}}
constexpr int m2 = n2; // expected-error {{constant expression}} expected-note {{read of volatile-qualified type 'const volatile int'}}
constexpr int m1b = const_cast<const int&>(n1); // expected-error {{constant expression}} expected-note {{read of volatile object 'n1'}}
constexpr int m2b = const_cast<const int&>(n2); // expected-error {{constant expression}} expected-note {{read of volatile object 'n2'}}

struct T { int n; };
const T t = { 42 };

constexpr int f(volatile int &&r) {
  return r; // expected-note {{read of volatile-qualified type 'volatile int'}}
}
constexpr int g(volatile int &&r) {
  return const_cast<int&>(r); // expected-note {{read of volatile temporary is not allowed in a constant expression}}
}
struct S {
  int j : f(0); // expected-error {{constant expression}} expected-note {{in call to 'f(0)'}}
  int k : g(0); // expected-error {{constant expression}} expected-note {{temporary created here}} expected-note {{in call to 'g(0)'}}
  int l : n3; // expected-error {{constant expression}} expected-note {{read of non-const variable}}
  int m : t.n; // expected-warning{{width of bit-field 'm' (42 bits)}} expected-warning{{expression is not an integral constant expression}} expected-note{{read of non-constexpr variable 't' is not allowed}}
};

}


namespace ExternConstexpr {
  extern constexpr int n = 0;
  extern constexpr int m; // expected-error {{constexpr variable declaration must be a definition}}
  void f() {
    extern constexpr int i; // expected-error {{constexpr variable declaration must be a definition}}
    constexpr int j = 0;
    constexpr int k; // expected-error {{default initialization of an object of const type}}
  }

  extern const int q;
  constexpr int g() { return q; }
  constexpr int q = g();
  static_assert(q == 0, "zero-initialization should precede static initialization");

  extern int r; // expected-note {{here}}
  constexpr int h() { return r; } // expected-error {{never produces a constant}} expected-note {{read of non-const}}

  struct S { int n; };
  extern const S s;
  constexpr int x() { return s.n; }
  constexpr S s = {x()};
  static_assert(s.n == 0, "zero-initialization should precede static initialization");
}


namespace ComplexConstexpr {
  constexpr _Complex float test1 = {}; // expected-warning {{'_Complex' is a C99 extension}}
  constexpr _Complex float test2 = {1}; // expected-warning {{'_Complex' is a C99 extension}}
  constexpr _Complex double test3 = {1,2}; // expected-warning {{'_Complex' is a C99 extension}}
  constexpr _Complex int test4 = {4}; // expected-warning {{'_Complex' is a C99 extension}}
  constexpr _Complex int test5 = 4; // expected-warning {{'_Complex' is a C99 extension}}
  constexpr _Complex int test6 = {5,6}; // expected-warning {{'_Complex' is a C99 extension}}
  typedef _Complex float fcomplex; // expected-warning {{'_Complex' is a C99 extension}}
  constexpr fcomplex test7 = fcomplex();

  constexpr const double &t2r = __real test3;
  constexpr const double &t2i = __imag test3;
  static_assert(&t2r + 1 == &t2i, "");
  static_assert(t2r == 1.0, "");
  static_assert(t2i == 2.0, "");
  constexpr const double *t2p = &t2r;
  static_assert(t2p[-1] == 0.0, ""); // expected-error {{constant expr}} expected-note {{cannot refer to element -1 of array of 2 elements}}
  static_assert(t2p[0] == 1.0, "");
  static_assert(t2p[1] == 2.0, "");
  static_assert(t2p[2] == 0.0, ""); // expected-error {{constant expr}} expected-note {{one-past-the-end pointer}}
  static_assert(t2p[3] == 0.0, ""); // expected-error {{constant expr}} expected-note {{cannot refer to element 3 of array of 2 elements}}
  constexpr _Complex float *p = 0; // expected-warning {{'_Complex' is a C99 extension}}
  constexpr float pr = __real *p; // expected-error {{constant expr}} expected-note {{cannot access real component of null}}
  constexpr float pi = __imag *p; // expected-error {{constant expr}} expected-note {{cannot access imaginary component of null}}
  constexpr const _Complex double *q = &test3 + 1; // expected-warning {{'_Complex' is a C99 extension}}
  constexpr double qr = __real *q; // expected-error {{constant expr}} expected-note {{cannot access real component of pointer past the end}}
  constexpr double qi = __imag *q; // expected-error {{constant expr}} expected-note {{cannot access imaginary component of pointer past the end}}

  static_assert(__real test6 == 5, "");
  static_assert(__imag test6 == 6, "");
  static_assert(&__imag test6 == &__real test6 + 1, "");
}

namespace ConvertedConstantExpr {
  extern int &m;
  extern int &n;

  constexpr int k = 4;
  int &m = const_cast<int&>(k);

  // If we have nothing more interesting to say, ensure we don't produce a
  // useless note and instead just point to the non-constant subexpression.
  enum class E {
    em = m,
    en = n, // expected-error {{not a constant expression}}
    eo = (m +
          n // expected-error {{not a constant expression}}
          ),
    eq = reinterpret_cast<long>((int*)0) // expected-error {{not a constant expression}} expected-note {{reinterpret_cast}}
  };
}

// DR1405: don't allow reading mutable members in constant expressions.
namespace MutableMembers {
  struct MM {
    mutable int n; // expected-note 3{{declared here}}
  } constexpr mm = { 4 };
  constexpr int mmn = mm.n; // expected-error {{constant expression}} expected-note {{read of mutable member 'n' is not allowed in a constant expression}}
  int x = (mm.n = 1, 3);
  constexpr int mmn2 = mm.n; // expected-error {{constant expression}} expected-note {{read of mutable member 'n' is not allowed in a constant expression}}

  // Here's one reason why allowing this would be a disaster...
  template<int n> struct Id { int k = n; };
  int f() {
    constexpr MM m = { 0 };
    ++m.n;
    return Id<m.n>().k; // expected-error {{not a constant expression}} expected-note {{read of mutable member 'n' is not allowed in a constant expression}}
  }

  struct A { int n; };
  struct B { mutable A a; }; // expected-note {{here}}
  struct C { B b; };
  constexpr C c[3] = {};
  constexpr int k = c[1].b.a.n; // expected-error {{constant expression}} expected-note {{mutable}}

  struct D { int x; mutable int y; }; // expected-note {{here}}
  constexpr D d1 = { 1, 2 };
  int l = ++d1.y;
  constexpr D d2 = d1; // expected-error {{constant}} expected-note {{mutable}} expected-note {{in call}}

  struct E {
    union {
      int a;
      mutable int b; // expected-note {{here}}
    };
  };
  constexpr E e1 = {{1}};
  constexpr E e2 = e1; // expected-error {{constant}} expected-note {{mutable}} expected-note {{in call}}

  struct F {
    union U { };
    mutable U u;
    struct X { };
    mutable X x;
    struct Y : X { X x; U u; };
    mutable Y y;
    int n;
  };
  // This is OK; we don't actually read any mutable state here.
  constexpr F f1 = {};
  constexpr F f2 = f1;

  struct G {
    struct X {};
    union U { X a; };
    mutable U u; // expected-note {{here}}
  };
  constexpr G g1 = {};
  constexpr G g2 = g1; // expected-error {{constant}} expected-note {{mutable}} expected-note {{in call}}
  constexpr G::U gu1 = {};
  constexpr G::U gu2 = gu1;

  union H {
    mutable G::X gx; // expected-note {{here}}
  };
  constexpr H h1 = {};
  constexpr H h2 = h1; // expected-error {{constant}} expected-note {{mutable}} expected-note {{in call}}
}

namespace Vector {
  typedef int __attribute__((vector_size(16))) VI4;
  constexpr VI4 f(int n) {
    return VI4 { n * 3, n + 4, n - 5, n / 6 };
  }
  constexpr auto v1 = f(10);

  typedef double __attribute__((vector_size(32))) VD4;
  constexpr VD4 g(int n) {
    return (VD4) { n / 2.0, n + 1.5, n - 5.4, n * 0.9 }; // expected-warning {{C99}}
  }
  constexpr auto v2 = g(4);
}


namespace Lifetime {
  void f() {
    constexpr int &n = n; // expected-error {{constant expression}} expected-note {{use of reference outside its lifetime}} expected-warning {{not yet bound to a value}}
    constexpr int m = m; // expected-error {{constant expression}} expected-note {{read of object outside its lifetime}}
  }

  constexpr int &get(int &&n) { return n; }
  constexpr int &&get_rv(int &&n) { return static_cast<int&&>(n); }
  struct S {
    int &&r;
    int &s;
    int t;
    constexpr S() : r(get_rv(0)), s(get(0)), t(r) {} // expected-note {{read of object outside its lifetime}}
    constexpr S(int) : r(get_rv(0)), s(get(0)), t(s) {} // expected-note {{read of object outside its lifetime}}
  };
  constexpr int k1 = S().t; // expected-error {{constant expression}} expected-note {{in call}}
  constexpr int k2 = S(0).t; // expected-error {{constant expression}} expected-note {{in call}}

  struct Q {
    int n = 0;
    constexpr int f() const { return 0; }
  };
  constexpr Q *out_of_lifetime(Q q) { return &q; } // expected-warning {{address of stack}} expected-note 2{{declared here}}
  constexpr int k3 = out_of_lifetime({})->n; // expected-error {{constant expression}} expected-note {{read of variable whose lifetime has ended}}
  constexpr int k4 = out_of_lifetime({})->f(); // expected-error {{constant expression}} expected-note {{member call on variable whose lifetime has ended}}

  constexpr int null = ((Q*)nullptr)->f(); // expected-error {{constant expression}} expected-note {{member call on dereferenced null pointer}}

  Q q;
  Q qa[3];
  constexpr int pte0 = (&q)[0].f(); // ok
  constexpr int pte1 = (&q)[1].f(); // expected-error {{constant expression}} expected-note {{member call on dereferenced one-past-the-end pointer}}
  constexpr int pte2 = qa[2].f(); // ok
  constexpr int pte3 = qa[3].f(); // expected-error {{constant expression}} expected-note {{member call on dereferenced one-past-the-end pointer}}

  constexpr Q cq;
  constexpr Q cqa[3];
  constexpr int cpte0 = (&cq)[0].f(); // ok
  constexpr int cpte1 = (&cq)[1].f(); // expected-error {{constant expression}} expected-note {{member call on dereferenced one-past-the-end pointer}}
  constexpr int cpte2 = cqa[2].f(); // ok
  constexpr int cpte3 = cqa[3].f(); // expected-error {{constant expression}} expected-note {{member call on dereferenced one-past-the-end pointer}}

  // FIXME: There's no way if we can tell if the first call here is valid; it
  // depends on the active union member. Should we reject for that reason?
  union U {
    int n;
    Q q;
  };
  U u1 = {0};
  constexpr U u2 = {0};
  constexpr int union_member1 = u1.q.f();
  constexpr int union_member2 = u2.q.f(); // expected-error {{constant expression}} expected-note {{member call on member 'q' of union with active member 'n'}}

  struct R { // expected-note {{field init}}
    struct Inner { constexpr int f() const { return 0; } };
    int a = b.f(); // expected-warning {{uninitialized}} expected-note {{member call on object outside its lifetime}}
    Inner b;
  };
  // FIXME: This should be rejected under DR2026.
  constexpr R r; // expected-note {{default constructor}}
  void rf() {
    constexpr R r; // expected-error {{constant expression}} expected-note {{in call}}
  }
}
namespace ZeroSizeTypes {
  constexpr int (*p1)[0] = 0, (*p2)[0] = 0;
  constexpr int k = p2 - p1;
  // expected-error@-1 {{constexpr variable 'k' must be initialized by a constant expression}}
  // expected-note@-2 {{subtraction of pointers to type 'int [0]' of zero size}}

  int arr[5][0];
  constexpr int f() { // expected-error {{never produces a constant expression}}
    return &arr[3] - &arr[0]; // expected-note {{subtraction of pointers to type 'int [0]' of zero size}}
  }
}


namespace PR21786 {
  extern void (*start[])();
  extern void (*end[])();
  static_assert(&start != &end, ""); // expected-error {{constant expression}}
  static_assert(&start != nullptr, "");

  struct Foo;
  struct Bar {
    static const Foo x;
    static const Foo y;
  };
  static_assert(&Bar::x != nullptr, "");
  static_assert(&Bar::x != &Bar::y, "");
}

namespace IncompleteClass {
  struct XX {
    static constexpr int f(XX*) { return 1; } // expected-note {{here}}
    friend constexpr int g(XX*) { return 2; } // expected-note {{here}}

    static constexpr int i = f(static_cast<XX*>(nullptr)); // expected-error {{constexpr variable 'i' must be initialized by a constant expression}}  expected-note {{undefined function 'f' cannot be used in a constant expression}}
    static constexpr int j = g(static_cast<XX*>(nullptr)); // expected-error {{constexpr variable 'j' must be initialized by a constant expression}}  expected-note {{undefined function 'g' cannot be used in a constant expression}}
  };
}


namespace CompoundLiteral {
  // Matching GCC, file-scope array compound literals initialized by constants
  // are lifetime-extended.
  constexpr int *p = (int*)(int[1]){3}; // expected-warning {{C99}}
  static_assert(*p == 3, "");
  static_assert((int[2]){1, 2}[1] == 2, ""); // expected-warning {{C99}}

  // Other kinds are not.
  struct X { int a[2]; };
  constexpr int *n = (X){1, 2}.a; // expected-warning {{C99}} expected-warning {{temporary}}
  // expected-error@-1 {{constant expression}}
  // expected-note@-2 {{pointer to subobject of temporary}}
  // expected-note@-3 {{temporary created here}}

  void f() {
    static constexpr int *p = (int*)(int[1]){3}; // expected-warning {{C99}} expected-warning {{temporary}}
    // expected-error@-1 {{constant expression}}
    // expected-note@-2 {{pointer to subobject of temporary}}
    // expected-note@-3 {{temporary created here}}
    static_assert((int[2]){1, 2}[1] == 2, ""); // expected-warning {{C99}}
  }
}

namespace CaseStatements {
  int x;
  void f(int n) {
    switch (n) {
    case MemberZero().zero: // expected-error {{did you mean to call it with no arguments?}} expected-note {{previous}}
    case id(0): // expected-error {{duplicate case value '0'}}
      return;
    case __builtin_constant_p(true) ? (__SIZE_TYPE__)&x : 0:; // expected-error {{constant}}
    }
  }
}

namespace Fold {

  // This macro forces its argument to be constant-folded, even if it's not
  // otherwise a constant expression.
  #define fold(x) (__builtin_constant_p(x) ? (x) : (x))

  constexpr int n = (long)(char*)123; // expected-error {{constant expression}} expected-note {{reinterpret_cast}}
  constexpr int m = fold((long)(char*)123); // ok
  static_assert(m == 123, "");

  #undef fold

}

namespace StmtExpr {
  struct A { int k; };
  void f() {
    static_assert(({ const int x = 5; x * 3; }) == 15, ""); // expected-warning {{extension}}
    constexpr auto a = ({ A(); }); // expected-warning {{extension}}
  }
  constexpr int g(int k) {
    return ({ // expected-warning {{extension}}
      const int x = k;
      x * x;
    });
  }
  static_assert(g(123) == 15129, "");
  constexpr int h() { // expected-error {{never produces a constant}}
    return ({ // expected-warning {{extension}}
      return 0; // expected-note {{not supported}}
      1;
    });
  }
}

namespace NeverConstantTwoWays {
  // If we see something non-constant but foldable followed by something
  // non-constant and not foldable, we want the first diagnostic, not the
  // second.
  constexpr int f(int n) { // expected-error {{never produces a constant expression}}
    return (int *)(long)&n == &n ? // expected-note {{reinterpret_cast}}
        1 / 0 : // expected-warning {{division by zero}}
        0;
  }

  constexpr int n = // expected-error {{must be initialized by a constant expression}}
      (int *)(long)&n == &n ? // expected-note {{reinterpret_cast}}
        1 / 0 :
        0;
}
