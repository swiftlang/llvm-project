// RUN: %clang_cc1 -fsyntax-only -Wownership-inference -Wownership-inference-suggestions %s -verify

// A destructor-managed raw pointer makes the owner as self-contained as the
// object it owns: owning a view yields a view, like embedding it by value.

template<typename T> void derefIfNotNull(T *p) { if (p) p->deref(); }

// expected-warning@+1 {{type 'String' was inferred as 'view'}}
struct String { const char *characters; }; // expected-note {{holds unmanaged raw-pointer field 'characters'}}

// expected-warning@+1 {{type 'Parser' was inferred as 'view'}}
struct Parser {
  void deref() const { if (!--rc) delete this; }
  mutable unsigned rc{1};
  const String &s; // expected-note {{holds reference field 's'}}
};

struct Counted {
// expected-warning@-1 {{type 'Counted' was inferred as 'self-contained'}}
  void deref() const { if (!--rc) delete this; }
  mutable unsigned rc{1};
};

template<typename T> struct Ref {
  ~Ref() { derefIfNotNull(m_ptr); }
  T *m_ptr; // expected-note 2 {{holds unmanaged raw-pointer field 'm_ptr'}}
};

// Owns a view.
template struct Ref<Parser>;
// expected-warning@-1 {{type 'Ref<Parser>' failed to infer}} FIXME: this should be a view because Parser is a view

// Owns a self-contained object.
template struct Ref<Counted>;
// expected-warning@-1 {{type 'Ref<Counted>' failed to infer}} FIXME: should be self-contained.

// An owned self-reference adds nothing to its own outcome.
struct SelfRecursiveNode {
// expected-warning@-1 {{type 'SelfRecursiveNode' failed to infer}} FIXME: should be self-contained.
  void deref() const { if (!--rc) delete this; }
  mutable unsigned rc{1};
  SelfRecursiveNode *next; // expected-note {{holds unmanaged raw-pointer field 'next'}}
  ~SelfRecursiveNode() { derefIfNotNull(next); }
};

// The pointee may be incomplete, unlike a by-value field: nothing is known.
struct Incomplete;
void derefIncomplete(Incomplete *) __attribute__((ownership_takes(release, 1)));
// expected-warning@+1 {{type 'OwnsIncomplete' failed to infer}}
struct OwnsIncomplete {
  ~OwnsIncomplete() { derefIncomplete(m_ptr); }
  Incomplete *m_ptr; // expected-note {{holds unmanaged raw-pointer field 'm_ptr'}}
};

// An annotation on the forward declaration is trusted even though the
// definition is never seen.
struct __attribute__((swift_attr("Escapable"))) AnnotatedSelfContained;
struct __attribute__((swift_attr("~Escapable"))) AnnotatedView;
void derefSC(AnnotatedSelfContained *) __attribute__((ownership_takes(release, 1)));
void derefView(AnnotatedView *) __attribute__((ownership_takes(release, 1)));

struct OwnsAnnotatedSelfContained {
// expected-warning@-1 {{type 'OwnsAnnotatedSelfContained' failed to infer}} FIXME: should be a view
  ~OwnsAnnotatedSelfContained() { derefSC(m_ptr); }
  AnnotatedSelfContained *m_ptr; // expected-note {{holds unmanaged raw-pointer field 'm_ptr'}}
};

// expected-warning@+1 {{type 'OwnsAnnotatedView' failed to infer}} FIXME: should be a view
struct OwnsAnnotatedView {
  ~OwnsAnnotatedView() { derefView(m_ptr); }
  AnnotatedView *m_ptr; // expected-note {{holds unmanaged raw-pointer field 'm_ptr'}}
};
