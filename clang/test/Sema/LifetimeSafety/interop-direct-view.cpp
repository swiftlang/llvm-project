// RUN: rm -rf %t && split-file %s %t
// RUN: %clang_cc1 -I %t -std=c++17 -fsyntax-only -finterop-type-inference -Rinterop-type-inference-direct-view -verify %t/main.cpp
// RUN: %clang_cc1 -I %t -std=c++17 -fsyntax-only -finterop-type-inference -Rinterop-type-inference-direct-view-explain -verify=explain %t/main.cpp
// RUN: %clang_cc1 -I %t -std=c++17 -fsyntax-only -finterop-type-inference -Winterop-type-inference-contradicts-existing-attributes -verify=attr %t/main.cpp

//--- main.cpp
#include "std_span_string_view.h"

class V { // expected-remark {{'V' is a direct view type}}
  int *p;
public:
  V(int *q) : p(q) {}
};

int *g;
// Other parameters do not matter.
class Escape { // expected-remark {{'Escape' is a direct view type}}
  int *p;
public:
  Escape(int *q, int *r) : p(q) { g = r; }
};

class Base { // expected-remark {{'Base' is a direct view type}}
protected:
  int *p;
public:
  Base(int *q) : p(q) {}
};
// A direct view base is the borrow capturer.
class Derived : Base { // expected-remark {{'Derived' is a direct view type}}
public:
  Derived(int *q) : Base(q) {}
};

class DerivedBorrows : Base { // explain-remark {{'DerivedBorrows' is not a direct view type}}
  int *r;
public:
  DerivedBorrows(int *q) : Base(q), r(q) {} // explain-note {{member function captures an owning pointer parameter in a field, while the direct view base class 'Base' is the borrow capturer}}
};

// Invalidating a field of the base invalidates the base.
class DerivedDeletes : Base { // explain-remark {{'DerivedDeletes' is not a direct view type}}
public:
  DerivedDeletes(int *q) : Base(q) {}
  void reset() { delete p; } // explain-note {{this member function may invalidate 'Base'}}
};

class TwoFields { // explain-remark {{'TwoFields' is not a direct view type}}
  int *p, *q;
public:
  TwoFields(int *a) : p(a), q(a) {} // explain-note {{member function captures parameters in multiple fields}}
};

class NoCapture { // explain-remark {{'NoCapture' is not a direct view type}} explain-note {{no member function captures an owning pointer parameter in a field}}
  int *p;
public:
  NoCapture(int *q) : p(nullptr) {}
};

// A member function that captures nothing does not matter.
class OtherField { // expected-remark {{'OtherField' is a direct view type}}
  int *p;
public:
  OtherField(int *a) : p(a) {}
  OtherField(int *a, int) : p(nullptr) {}
};

// An ordinary method can capture, too.
class Setter { // expected-remark {{'Setter' is a direct view type}}
  int *p = nullptr;
public:
  void set(int *a) { p = a; }
};

class Inconsistent { // explain-remark {{'Inconsistent' is not a direct view type}}
  int *p, *q;
public:
  Inconsistent(int *a) : p(a), q(nullptr) {}
  void set(int *a) { q = a; } // explain-note {{member function captures an owning pointer parameter in a field other than 'p', where another member function captures one}}
};

// A pointer to a type that is explicitly Swift escapable is an owning pointer.
struct __attribute__((swift_attr("Escapable"))) Esc { // explain-remark {{'Esc' is not a direct view type}} explain-note {{no member function captures an owning pointer parameter in a field}}
  int i;
};
struct Plain { // explain-remark {{'Plain' is not a direct view type}} explain-note {{no member function captures an owning pointer parameter in a field}}
  int i;
};

class EscPtr { // expected-remark {{'EscPtr' is a direct view type}}
  Esc *p;
public:
  EscPtr(Esc *q) : p(q) {}
};

class PlainPtr { // explain-remark {{'PlainPtr' is not a direct view type}} explain-note {{no member function captures an owning pointer parameter in a field}}
  Plain *p;
public:
  PlainPtr(Plain *q) : p(q) {}
};

class Deletes { // explain-remark {{'Deletes' is not a direct view type}}
  int *p;
public:
  Deletes(int *q) : p(q) {}
  void reset() { delete p; } // explain-note {{this member function may invalidate 'p'}}
};

class DeletesInDtor { // explain-remark {{'DeletesInDtor' is not a direct view type}}
  int *p;
public:
  DeletesInDtor(int *q) : p(q) {}
  ~DeletesInDtor() { delete p; } // explain-note {{this member function may invalidate 'p'}}
};

void release(int *x) { delete x; }

// The invalidation reaches the dtor through the summary of the callee.
class DeletesInCallee { // explain-remark {{'DeletesInCallee' is not a direct view type}}
  int *p;
public:
  DeletesInCallee(int *q) : p(q) {}
  ~DeletesInCallee() { release(p); } // explain-note {{this member function may invalidate 'p'}}
};

class DeletesOther { // expected-remark {{'DeletesOther' is a direct view type}}
  int *p;
public:
  DeletesOther(int *q) : p(q) {}
  void other(int *x) { delete x; }
};

// X is analyzed first and becomes an implicit gsl::Pointer, so Y captures into
// its field `x`.
class X { // expected-remark {{'X' is a direct view type}}
  int *p;
public:
  X(int *q) : p(q) {}
};
class Y { // expected-remark {{'Y' is a direct view type}}
  X x;
public:
  Y(int *q) : x(q) {}
};

// A single public pointer-like field makes a direct view without a capturing
// member function.
struct [[gsl::Pointer]] PointerThinWrapper { // expected-remark {{'PointerThinWrapper' is a direct view type}}
  int *p;
};

// A type annotated gsl::Pointer that is not inferred to be a direct view.
struct [[gsl::Pointer]] Annotated { // explain-remark {{'Annotated' is not a direct view type}} explain-note {{there is no single public pointer-like field}} attr-warning {{'Annotated' is annotated 'gsl::Pointer' but is not inferred to be a direct view type}}
  int *p, *q;
};

// A type annotated self-contained that is inferred to be a direct view.
struct __attribute__((swift_attr("Escapable"))) EscView { // expected-remark {{'EscView' is a direct view type}} attr-warning {{'EscView' is annotated self-contained ('swift_attr("Escapable")') but is inferred to be a direct view type}}
  int *p;
};

// Instantiates the templates in the header.
int foo(std::span<int> sp, std::string_view sv) {
  int arr[3] = {1, 2, 3};
  std::span<int> whole(arr);
  std::span<int> sub = whole.subspan(1, 2).first(1);
  std::string_view tail = sv.substr(1, 2);
  tail.remove_prefix(1);
  return sp.size() + sub[0] + static_cast<int>(tail.size()) + (sv.empty() ? 0 : 1);
}

//--- std_span_string_view.h
// Self-contained span and string_view for testing. No includes.
namespace std {
using size_t = decltype(sizeof(0));

template <class CharT> struct char_traits;
template <> struct char_traits<char> { // explain-remark {{'char_traits<char>' is not a direct view type}} explain-note {{no member function captures an owning pointer parameter in a field}}
  static constexpr size_t length(const char *s) {
    size_t n = 0;
    while (s[n]) ++n;
    return n;
  }
};

template <class T>
struct [[gsl::Pointer()]] span { // expected-remark {{'span<int>' is a direct view type}}
  T *ptr = nullptr;
  size_t len = 0;

  constexpr span() = default;
  constexpr span(T *p, size_t n) : ptr(p), len(n) {}
  template <size_t N> constexpr span(T (&arr)[N]) : ptr(arr), len(N) {}

  constexpr T *data() const { return ptr; }
  constexpr size_t size() const { return len; }
  constexpr bool empty() const { return len == 0; }
  constexpr T &operator[](size_t i) const { return ptr[i]; }
  constexpr T *begin() const { return ptr; }
  constexpr T *end() const { return ptr + len; }
  constexpr span first(size_t n) const { return span(ptr, n); }
  constexpr span subspan(size_t off, size_t n) const {
    return span(ptr + off, n);
  }
};

template <class CharT>
struct [[gsl::Pointer()]] basic_string_view { // expected-remark {{'basic_string_view<char>' is a direct view type}}
  const CharT *ptr = nullptr;
  size_t len = 0;

  constexpr basic_string_view() = default;
  constexpr basic_string_view(const CharT *s)
      : ptr(s), len(char_traits<CharT>::length(s)) {}
  constexpr basic_string_view(const CharT *p, size_t n) : ptr(p), len(n) {}

  constexpr const CharT *data() const { return ptr; }
  constexpr size_t size() const { return len; }
  constexpr bool empty() const { return len == 0; }
  constexpr const CharT &operator[](size_t i) const { return ptr[i]; }
  constexpr const CharT *begin() const { return ptr; }
  constexpr const CharT *end() const { return ptr + len; }
  constexpr basic_string_view substr(size_t pos, size_t n) const {
    return basic_string_view(ptr + pos, n);
  }
  constexpr void remove_prefix(size_t n) { ptr += n; len -= n; }
  constexpr void remove_suffix(size_t n) { len -= n; }
};
using string_view = basic_string_view<char>;
} // namespace std
