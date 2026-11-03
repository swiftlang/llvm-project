// RUN: %clang_cc1 -std=c++2b -fsyntax-only \
// RUN:     -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis \
// RUN:     -Wlifetime-safety-experimental-field-origin-escape \
// RUN:     %s -verify -Wownership-inference

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
  T *m_ptr; // expected-note 2 {{holds managed raw-pointer field 'm_ptr'}}
            // expected-note@-1 {{owns pointee of type 'Parser' through field 'm_ptr' that was inferred as 'view'}}
};

// Owns a view.
template struct Ref<Parser>;
// expected-warning@-1 {{type 'Ref<Parser>' was inferred as 'view'}}

// Owns a self-contained object.
template struct Ref<Counted>;
// expected-warning@-1 {{type 'Ref<Counted>' was inferred as 'self-contained'}}

// An owned self-reference adds nothing to its own verdict.
struct Node {
// expected-warning@-1 {{type 'Node' was inferred as 'self-contained'}}
  void deref() const { if (!--rc) delete this; }
  mutable unsigned rc{1};
  Node *next; // expected-note {{holds managed raw-pointer field 'next'}}
  ~Node() { derefIfNotNull(next); }
};

// The pointee may be incomplete, unlike a by-value field: nothing is known.
struct Incomplete;
void derefIncomplete(Incomplete *) __attribute__((ownership_takes(release, 1)));
// expected-warning@+1 {{type 'OwnsIncomplete' failed to infer}}
struct OwnsIncomplete {
  ~OwnsIncomplete() { derefIncomplete(m_ptr); }
  Incomplete *m_ptr; // expected-note {{holds managed raw-pointer field 'm_ptr'}}
                     // expected-note@-1 {{owns pointee of type 'Incomplete' through field 'm_ptr' that was inferred as 'unknown'}}
};

// An annotation on the forward declaration is trusted even though the
// definition is never seen.
struct __attribute__((swift_attr("Escapable"))) AnnotatedSelfContained;
struct __attribute__((swift_attr("~Escapable"))) AnnotatedView;
void derefSC(AnnotatedSelfContained *) __attribute__((ownership_takes(release, 1)));
void derefView(AnnotatedView *) __attribute__((ownership_takes(release, 1)));

struct OwnsAnnotatedSelfContained {
// expected-warning@-1 {{type 'OwnsAnnotatedSelfContained' was inferred as 'self-contained'}}
  ~OwnsAnnotatedSelfContained() { derefSC(m_ptr); }
  AnnotatedSelfContained *m_ptr; // expected-note {{holds managed raw-pointer field 'm_ptr'}}
};

// expected-warning@+1 {{type 'OwnsAnnotatedView' was inferred as 'view'}}
struct OwnsAnnotatedView {
  ~OwnsAnnotatedView() { derefView(m_ptr); }
  AnnotatedView *m_ptr; // expected-note {{holds managed raw-pointer field 'm_ptr'}}
                        // expected-note@-1 {{owns pointee of type 'AnnotatedView' through field 'm_ptr' that was annotated as 'view'}}
};
