// RUN: %clang_cc1 -fsyntax-only -std=c++23 -finterop-lifetime-inference-test-mode -Rinterop-lifetime-inference-test-mode -verify %s

// Code from other LSA tests whose hand-written annotations the checker accepts
// without warnings. In test mode those annotations are dropped and become
// oracles, so inference should recover every one of them.

#include "Inputs/lifetime-analysis.h"

// expected-remark@*:* {{lifetime attribute inference vs. already-written oracle: matched 43; failed to infer (lifetimebound 7, noescape 0, lifetime_capture_by 5); inferred 58 in total}}

// The expected notes mark the oracles that inference misses so far: callers do
// not use the summaries of callees, and referent-level flow is not inferred.

// From lifetimebound-violation.cpp.
namespace lifetimebound_violation {
struct [[gsl::Owner]] MyObj {
  int id;
  ~MyObj() {}  // Non-trivial destructor
};

struct [[gsl::Pointer()]] View {
  View(const MyObj &); // Borrows from MyObj
  View();
  void use() const;
};

// Bodies added: the original declarations are bodyless, so test mode would
// drop their annotations with nothing to infer them from.
View lb(const MyObj &obj [[clang::lifetimebound]]) { return obj; }

View lb_view(View v [[clang::lifetimebound]]) { return v; }

View return_through_lifetimebound_passthrough(
    const MyObj &obj [[clang::lifetimebound]]) { // expected-note {{'clang::lifetimebound'}}
  return lb(obj);
}

View return_through_alias(const MyObj &obj [[clang::lifetimebound]]) {
  const MyObj &alias = obj;
  return alias;
}

View return_through_two_lifetimebound_calls(
    const MyObj &obj [[clang::lifetimebound]]) { // expected-note {{'clang::lifetimebound'}}
  return lb_view(lb(obj));
}

View return_constructed_view_through_lifetimebound_forwarder(
    const MyObj &obj [[clang::lifetimebound]]) { // expected-note {{'clang::lifetimebound'}}
  return lb_view(View(obj));
}

struct GoodThisReturn {
  MyObj data;

  View get() const [[clang::lifetimebound]] {
    return data;
  }
};

struct ViewWithMember {
  ViewWithMember(const char *data [[clang::lifetimebound]]) : mData(data) {}
  const char *mData;
};
} // namespace lifetimebound_violation

// From noescape-violation.cpp.
namespace noescape_violation {
struct [[gsl::Owner]] MyObj {
  int id;
  ~MyObj() {}  // Non-trivial destructor
};

struct [[gsl::Pointer()]] View {
  View(const MyObj&); // Borrows from MyObj
  View();
  void use() const;
};

View mixed_only_noescape_escapes(
    const MyObj& a [[clang::noescape]],
    const MyObj& b [[clang::lifetimebound]]) {
  use(a);
  return b;
}

void use_locally(const MyObj& in [[clang::noescape]]) {
  View v = in;
  v.use();
}

View return_with_lifetimebound(const MyObj& in [[clang::lifetimebound]]) {
  return in;
}

void pointer_used_locally(MyObj* p [[clang::noescape]]) {
  p->id = 42;
}

View identity_lifetimebound(View v [[clang::lifetimebound]]) { return v; }

struct Container {
  MyObj data;
  const MyObj& getRef() const [[clang::lifetimebound]] { return data; }
};
} // namespace noescape_violation

// From safety.cpp.
namespace safety {
struct View;

struct [[gsl::Owner]] MyObj {
  int id;
  MyObj();
  ~MyObj() {}  // Non-trivial destructor
};

struct [[gsl::Pointer()]] View {
  View(const MyObj&); // Borrows from MyObj
  View();
  void use() const;
};

View construct_view(const MyObj &obj [[clang::lifetimebound]]) {
  return View(obj);
}

struct PtrHolder {
  int* ptr;
  int* const& getRef() const [[clang::lifetimebound]] { return ptr; }
};

struct [[gsl::Pointer]] TrueView {
  const int *p;
  constexpr TrueView(const int &x [[clang::lifetimebound]]) : p(&x) {}
  constexpr explicit operator bool() const { return true; }
};
struct [[gsl::Pointer]] FalseView {
  const int *p;
  constexpr FalseView(const int &x [[clang::lifetimebound]]) : p(&x) {}
  constexpr explicit operator bool() const { return false; }
};

struct MyObjStorage {
  MyObj objs[1];
  MyObjStorage() {}
  ~MyObjStorage() {}
  const MyObj *begin() const [[clang::lifetimebound]]  { return objs; }
  const MyObj *end() const [[clang::lifetimebound]] { return objs + 1; }
};

struct SWithUserDefinedCopyLikeOps {
  SWithUserDefinedCopyLikeOps();
  SWithUserDefinedCopyLikeOps(const std::string &s [[clang::lifetimebound]]) : owned(s), data(s) {}

  SWithUserDefinedCopyLikeOps(const SWithUserDefinedCopyLikeOps &other [[clang::noescape]]) : owned("copy"), data(owned) {}

  SWithUserDefinedCopyLikeOps &operator=([[clang::noescape]] const SWithUserDefinedCopyLikeOps &) {
    owned = "copy";
    data = owned;
    return *this;
  }

  std::string owned;
  std::string_view data;
};

struct SWithOriginPropagatingCopy {
  SWithOriginPropagatingCopy();
  SWithOriginPropagatingCopy(const std::string &s [[clang::lifetimebound]]) : data(s) {}
  SWithOriginPropagatingCopy(const SWithOriginPropagatingCopy &other [[clang::noescape]]) : data(other.data) {}
  std::string_view data;
};

struct DefaultedOuter {
  DefaultedOuter();
  DefaultedOuter(const std::string &s [[clang::lifetimebound]]) : inner(s) {} // expected-note {{'clang::lifetimebound'}}
  SWithUserDefinedCopyLikeOps inner;
};

std::function<void()> capture_lifetimebound_param(int &x [[clang::lifetimebound]]) {
  return [&]() { (void)x; };
}
} // namespace safety

// From inapplicable-lifetimebound.cpp.
namespace inapplicable_lifetimebound {
struct [[gsl::Owner]] Owner {};

struct [[gsl::Pointer()]] View {
  View();
};

Owner *owner_ref(Owner &o [[clang::lifetimebound]]) {
  return &o;
}

const Owner *const_owner_ref(const Owner &o [[clang::lifetimebound]]) {
  return &o;
}

Owner *owner_ptr(Owner *o [[clang::lifetimebound]]) {
  return o;
}

int *scalar_ptr(int *p [[clang::lifetimebound]]) {
  return p;
}

View view_value(View v [[clang::lifetimebound]]) {
  return v;
}
} // namespace inapplicable_lifetimebound

// From explicit-object-param-no-crash.cpp.
namespace explicit_object_param {
struct Holder {
  int field;
  const int *borrow() [[clang::lifetimebound]] { return &field; }
  void consume(this Holder &&self [[clang::noescape]]) {}
};

const int *object_arg_is_not_moved(Holder &&h [[clang::lifetimebound]]) { // expected-note {{'clang::lifetimebound'}}
  static_cast<Holder &&>(h).consume();
  return h.borrow();
}
} // namespace explicit_object_param

// From annotation-suggestions.cpp.
namespace annotation_suggestions {
struct View;

struct [[gsl::Owner]] MyObj {
  int id;
  MyObj(int i) : id(i) {}
  MyObj() {}
  ~MyObj() {}  // Non-trivial destructor
};

struct [[gsl::Pointer()]] View {
  View(const MyObj&); // Borrows from MyObj
  View();
  void use() const;
};

struct Container {
  MyObj data;
  const MyObj& getData() [[clang::lifetimebound]] { return data; }
};

struct ReturnThisAnnotated {
  const ReturnThisAnnotated& get() [[clang::lifetimebound]] { return *this; }
};

View already_annotated(View a [[clang::lifetimebound]]) {
 return a;
}

namespace make_unique_suggestion {
struct LifetimeBoundCtor {
  View v;
  LifetimeBoundCtor(const MyObj& obj [[clang::lifetimebound]]): v(obj) {}
};
} // namespace make_unique_suggestion

namespace new_allocation_suggestion {
struct LifetimeBoundCtor {
  View v;
  LifetimeBoundCtor();
  LifetimeBoundCtor(const MyObj& obj [[clang::lifetimebound]]) : v(obj) {}
};
} // namespace new_allocation_suggestion
} // namespace annotation_suggestions

// From SemaCXX/attr-lifetime-capture-by.cpp, the only function definitions in
// the repo with lifetime_capture_by, minus the attribute errors that file tests
// for.
namespace capture_by {
struct S {
  const int *x;
  void captureInt(const int &x [[clang::lifetime_capture_by_this]]) { this->x = &x; }
};

void nonMember(const int &x1 [[clang::lifetime_capture_by(s)]], // expected-note {{'clang::lifetime_capture_by'}}
               S &s [[clang::noescape]]) {
  s.captureInt(x1);
}

struct T {
  void member(const int &x [[clang::lifetime_capture_by(s)]], // expected-note {{'clang::lifetime_capture_by'}}
              S &s [[clang::noescape]]) {
    s.captureInt(x);
  }
};

struct SS {
  std::string_view v;
  void capture(const std::string &x [[clang::lifetime_capture_by_this]]) { v = x; }
};

void f_capture(const std::string &x [[clang::lifetime_capture_by(s)]], // expected-note {{'clang::lifetime_capture_by'}}
               SS &s [[clang::noescape]]) {
  s.capture(x);
}
} // namespace capture_by

// The pointer that `ptr` refers to flows to the return value of `exchange`,
// i.e. one indirection level into the argument. Callers must infer the same
// attributes as if they used the pointer itself. `exchange` has no oracle.
namespace inner_level {
int *exchange(int *&ptr, int *newValue) {
  int *oldValue = ptr;
  ptr = newValue;
  return oldValue;
}

int *ret(int *p [[clang::lifetimebound]]) { return exchange(p, nullptr); } // expected-note {{'clang::lifetimebound'}}

struct Escape {
  int *f;
  Escape(int *p [[clang::lifetimebound]]) : f(exchange(p, nullptr)) {} // expected-note {{'clang::lifetimebound'}}
};

void capture_by_param(int *p [[clang::lifetime_capture_by(out)]], // expected-note {{'clang::lifetime_capture_by'}}
                      int *&out [[clang::noescape]]) {
  out = exchange(p, nullptr);
}

struct CaptureByThis {
  int *f;
  void capture(int *p [[clang::lifetime_capture_by_this]]) { // expected-note {{'clang::lifetime_capture_by_this'}}
    f = exchange(p, nullptr);
  }
};

// The pointee of `pp` reaching the return value does not make `pp` itself
// escape.
int *noescape_inner(int *&pp [[clang::noescape]]) { return pp; }
} // namespace inner_level
