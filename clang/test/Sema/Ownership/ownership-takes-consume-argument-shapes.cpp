// RUN: %clang_cc1 -fsyntax-only -Wownership-inference -Wownership-inference-suggestions %s -verify

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
// expected-warning@-1 {{type 'LocalAlias' failed to infer}} FIXME: should be self-contained.
    ~LocalAlias() { Counted *p = m_ptr; derefIfNotNull(p); }
    Counted *m_ptr; // expected-note {{holds unmanaged raw-pointer field 'm_ptr'}}
};

// Argument is a MemberExpr (the field passed directly).
struct DirectField {
// expected-warning@-1 {{type 'DirectField' failed to infer}} FIXME: should be self-contained.
    ~DirectField() { derefIfNotNull(m_ptr); }
    Counted *m_ptr; // expected-note {{holds unmanaged raw-pointer field 'm_ptr'}}
};

// Argument is a CallExpr result (the ~RefPtr shape): no intervening local.
struct CallResult {
// expected-warning@-1 {{type 'CallResult' failed to infer}} FIXME: should be self-contained.
    ~CallResult() { derefIfNotNull(exchangePtr(m_ptr, nullptr)); }
    Counted *m_ptr; // expected-note {{holds unmanaged raw-pointer field 'm_ptr'}}
};
