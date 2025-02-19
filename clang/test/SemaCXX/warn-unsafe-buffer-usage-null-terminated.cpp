// RUN: %clang_cc1 -fsyntax-only -std=c++20 -Wno-all -Wunsafe-buffer-usage -fsafe-buffer-usage-suggestions -fexperimental-bounds-safety-attributes -verify %s

#include <ptrcheck.h>

typedef unsigned size_t;
namespace std {
  template <typename CharT>
  struct basic_string {
    const CharT *data() const noexcept;
    CharT *data() noexcept;
    const CharT *c_str() const noexcept;
    size_t size() const noexcept;
    size_t length() const noexcept;
  };

  typedef basic_string<char> string;

  template <typename CharT>
  struct basic_string_view {
    basic_string_view(basic_string<CharT> str);
    const CharT *data() const noexcept;
    size_t size() const noexcept;
    size_t length() const noexcept;
  };

  typedef basic_string_view<char> string_view;
} // namespace std

void nt_parm(const char * __null_terminated);
const char * __null_terminated get_nt(const char *, size_t);

void basics(const char * cstr, size_t cstr_len, std::string cxxstr) {
  const char * __null_terminated p = "hello";   // safe init

  nt_parm(p);
  nt_parm(get_nt(cstr, cstr_len));

  const char * __null_terminated p2 = cxxstr.c_str(); // safe init
  const char * __null_terminated p3;

  p3 = cxxstr.c_str();         // safe assignment
  p3 = "hello";                // safe assignment
  p3 = p;                      // safe assignment
  p3 = get_nt(cstr, cstr_len); // safe assignment

  // expected-warning@+1 {{unsafe initialization of a pointer of '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  const char * __null_terminated p4 = cstr;  // warn

  // expected-warning@+1 {{unsafe assignment to a parameter of '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  nt_parm(cstr);                             // warn
  // expected-warning@+1 {{unsafe assignment to a pointer of '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  p4 = cstr;                                 // warn

  std::string_view view{cxxstr};

  // expected-warning@+1 {{unsafe assignment to a pointer of '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  p4 = view.data();                          // warn
  // expected-warning@+1 {{unsafe assignment to a parameter of '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  nt_parm(view.data());                      // warn

  const char * __null_terminated p5 = 0;                 // nullptr is ok
  // expected-warning@+1 {{unsafe initialization of a pointer of '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  const char * __null_terminated p6 = (const char *)1;   // other integer literal is unsafe
  // test arrays?
  // what if an NT pointer p gets p[n] = ...?
  // recognize of std::string::c_str() should be under Wunsafe-buffer-usage
  // Test compound initializer and constructor initializer
}

void test_explicit_cast(char * p, const char * q) {
    // expected-warning@+1 {{unsafe casting to '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  const char * __null_terminated nt = (const char * __null_terminated) p;
  // expected-warning@+1 {{C++ named cast to '__null_terminated' type is unsafe}}
  const char * __null_terminated nt2 = reinterpret_cast<const char * __null_terminated> (p);

    // expected-warning@+1 {{unsafe casting to '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  nt = (const char * __null_terminated) p;
  // expected-warning@+1 {{C++ named cast to '__null_terminated' type is unsafe}}
  nt2 = reinterpret_cast<const char * __null_terminated> (p);
  // expected-warning@+1 {{C++ named cast to '__null_terminated' type is unsafe}}
  nt2 = static_cast<const char * __null_terminated> (p);

  // expected-warning@+1 {{unsafe casting to '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  const char * __null_terminated nt3 = (const char * __null_terminated) q;
  // expected-warning@+1 {{C++ named cast to '__null_terminated' type is unsafe}}
  const char * __null_terminated nt4 = reinterpret_cast<const char * __null_terminated> (q);

  // expected-warning@+1 {{unsafe casting to '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  nt3 = (const char * __null_terminated) q;
  // expected-warning@+1 {{C++ named cast to '__null_terminated' type is unsafe}}
  nt4 = reinterpret_cast<const char * __null_terminated> (q);
  // expected-warning@+1 {{C++ named cast to '__null_terminated' type is unsafe}}
  nt4 = static_cast<const char * __null_terminated> (q);
}

const char * __null_terminated test_return(const char * p, char * q, std::string &str) {
  if (p)
    return p; // expected-warning {{unsafe return of '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  if (q)
    return q; // expected-warning {{unsafe return of '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  return str.c_str();
}

void test_array(char * cstr) {
  const char arr[__null_terminated 3] = {'h', 'i', '\0'};
  // expected-error@+1 {{array 'arr2' with '__terminated_by' attribute is initialized with an incorrect terminator (expected: 0; got 'i')}}
  const char arr2[__null_terminated 2] = {'h', 'i'};
  const char * __null_terminated arr3[] = {"hello", "world"};
  // expected-warning@+1 {{unsafe initialization of a pointer of '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  const char * __null_terminated arr4[] = {"hello", "world", cstr};

  // expected-warning@+1 {{unsafe assignment to a pointer of '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  arr3[0] = cstr;
}

struct T {
  int a;
  const char * __null_terminated p;
  struct TT {
    int a;
    const char * __null_terminated p;
  } tt;
};
void test_compound(char * cstr) {
  std::string str;
  T t = {42, "hello"};
  T t2 = {.a = 42};
  T t3 = {.p = str.c_str()};
  // expected-warning@+1 {{unsafe initialization of a pointer of '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  T t4 = {42, "hello", {.p = cstr}};

  // expected-warning@+1 {{unsafe initialization of a pointer of '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  t4 = (struct T){42, "hello", {.p = cstr}};
}

  // expected-warning@+3 {{unsafe initialization of a pointer of '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
class C {
  const char * __null_terminated p;
  const char * __null_terminated q = (char *) 1; // warn
  struct T t;
public:
  // expected-warning@+1 2{{unsafe initialization of a pointer of '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  C(char * p): p(p), t({0, p}) {};
  C(const char * __null_terminated p, struct T t);
};

void f(const C &c);
C test_class(char * cstr) {
  // expected-warning@+2 {{unsafe initialization of a pointer of '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  // expected-warning@+1 {{unsafe assignment to a parameter of '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  C c{cstr, {0, cstr}};
  // expected-warning@+2 {{unsafe initialization of a pointer of '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  // expected-warning@+1 {{unsafe assignment to a parameter of '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  C c1(cstr, {0, cstr});
  // expected-warning@+2 {{unsafe initialization of a pointer of '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  // expected-warning@+1 {{unsafe assignment to a parameter of '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  C *c2 = new C(cstr, {0, cstr});
  // expected-warning@+2 {{unsafe initialization of a pointer of '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  // expected-warning@+1 {{unsafe assignment to a parameter of '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  f(C{cstr, {0, cstr}});

  C("hello", {0, "hello"});
  if (1-1)
    return {cstr, {}}; // expected-warning {{unsafe assignment to a parameter of '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  return {"hello", {}};
}


// Test input/output __null_terminated parameter.
// expected-note@+1 {{candidate function not viable:}}
void g(const char * __null_terminated *p);
void test_output(const char * __null_terminated p) {
  const char * __null_terminated local_nt = p;
  const char * const_local;
  char * local;

  g(&local_nt); // safe
  // expected-error@+1 {{passing 'const char **' to parameter of incompatible type 'const char * __terminated_by(0)*' (aka 'const char **') that adds '__terminated_by' attribute is not allowed}}
  g(&const_local);
  // expected-error@+1 {{no matching function for call to 'g'}}
  g(&local);
}
