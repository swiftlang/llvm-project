// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify

struct S {
  const int x;
  constexpr S() : x(2147483647) {}
  constexpr int call0() { return x + x; }   // expected-note {{value 4294967294 is outside the range of representable values of type 'int'}}
  constexpr int call1() { return call0(); } // expected-note {{in call to '&S()->call0()'}}
  constexpr int call2() { return call1(); } // expected-note {{in call to '&S()->call1()'}}
};

constexpr int method_root() {
  return S().call2();   // expected-note {{in call to '&S()->call2()'}}
}

constexpr int x = method_root();   // expected-error {{constexpr variable 'x' must be initialized by a constant expression}}\
                                   // expected-note {{in call to 'method_root()'}}

struct P {
  const int x;
  constexpr P(int y) : x(y + y) {}; // expected-note {{value 4294967294 is outside the range of representable values of type 'int'}}
};

constexpr int cons_root() {
  return P(2147483647).x;       // expected-note {{in call to 'P(2147483647)'}}
}

constexpr int y = cons_root();  // expected-error {{constexpr variable 'y' must be initialized by a constant expression}}\
                                // expected-note {{in call to 'cons_root()'}}


constexpr int callee_ptr(int *beg, int *end) {
  const int x = 2147483647;
  *beg = x + x; // expected-warning {{overflow in expression; result is -2 with type 'int'}}\
                // expected-note 3{{value 4294967294 is outside the range of representable values of type 'int'}}
  return *beg;
}

constexpr int caller_ptr() {
  int x = 0;
  return callee_ptr(&x, &x + 1); // expected-note {{in call to 'callee_ptr(&x, &x + 1)'}}
}

constexpr int z = caller_ptr(); // expected-error {{constexpr variable 'z' must be initialized by a constant expression}}\
                                // expected-note {{in call to 'caller_ptr()'}}

struct Q {
  int x;
  constexpr Q() : x(0) {}
};

constexpr int caller_field() {
  Q q;
  return callee_ptr(&q.x, &q.x + 1); // expected-note {{in call to 'callee_ptr(&q.x, &q.x + 1)'}}
}

constexpr int a = caller_field();// expected-error {{constexpr variable 'a' must be initialized by a constant expression}}\
                                 // expected-note {{in call to 'caller_field()'}}

struct R {
  Q q;
  constexpr R() : q() {}
};


constexpr int caller_nested_field() {
  R r;
  return callee_ptr(&r.q.x, &r.q.x + 1); // expected-note {{in call to 'callee_ptr(&r.q.x, &r.q.x + 1)'}}
}

constexpr int b = caller_nested_field();// expected-error {{constexpr variable 'b' must be initialized by a constant expression}}\
                                 // expected-note {{in call to 'caller_nested_field()'}}
