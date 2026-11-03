// RUN: %clang_cc1 -std=c++2b -fsyntax-only \
// RUN:     -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis \
// RUN:     -Wlifetime-safety-experimental-field-origin-escape \
// RUN:     %s -verify -Wownership-inference

// A destructor that releases its raw-pointer field by handing the field's value
// to a consuming (`ownership_takes`, here inferred from `delete this`) callee is
// self-contained. The field's value reaches the callee through loan propagation,
// so recognition must not depend on how the argument is spelled: a local alias,
// the field accessed directly, or a call result. All three forms below release
// `m_ptr` and must be inferred 'self-contained'.

struct Counted {
// expected-warning@-1 {{type 'Counted' was inferred as 'self-contained'}}
    void deref() const { if (!--m_refCount) delete this; }
    mutable unsigned m_refCount { 1 };
};

template<typename T> void derefIfNotNull(T *p) { if (p) p->deref(); }
template<typename T> T *exchangePtr(T *&slot, decltype(nullptr)) {
    T *old = slot; slot = nullptr; return old;
}

// Argument is a DeclRefExpr (a local alias of the field).
struct LocalAlias {
// expected-warning@-1 {{type 'LocalAlias' was inferred as 'self-contained'}}
    ~LocalAlias() { Counted *p = m_ptr; derefIfNotNull(p); }
    Counted *m_ptr; // expected-note {{holds managed raw-pointer field 'm_ptr'}}
};

// Argument is a MemberExpr (the field passed directly).
struct DirectField {
// expected-warning@-1 {{type 'DirectField' was inferred as 'self-contained'}}
    ~DirectField() { derefIfNotNull(m_ptr); }
    Counted *m_ptr; // expected-note {{holds managed raw-pointer field 'm_ptr'}}
};

// Argument is a CallExpr result (the ~RefPtr shape): no intervening local.
struct CallResult {
// expected-warning@-1 {{type 'CallResult' was inferred as 'self-contained'}}
    ~CallResult() { derefIfNotNull(exchangePtr(m_ptr, nullptr)); }
    Counted *m_ptr; // expected-note {{holds managed raw-pointer field 'm_ptr'}}
};
