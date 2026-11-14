// RUN: %clang_cc1 -fsyntax-only -I %S/Inputs -Wownership-inference -Wownership-inference-suggestions %s -verify

#include "RealOwnershipAnnotations.h"
#include "RealOwnershipTakesAnnotations.h"

// A destructor-managed raw pointer makes the owner as self-contained as the
// object it owns: owning a view yields a view, like embedding it by value.

template<typename T> void derefIfNotNull(T *p) { if (p) p->deref(); }

// expected-warning@+2 {{type 'String' was inferred as 'view'}}
// expected-note@+1 {{holds unmanaged raw-pointer field 'characters'}}
struct String { const char *characters; };

struct Parser { // expected-warning {{type 'Parser' was inferred as 'view'}}
  void deref() const { if (!--rc) delete this; }
  mutable unsigned rc{1};
  const String &s; // expected-note {{holds reference field 's'}}
};

struct Counted { // expected-warning {{type 'Counted' was inferred as 'self-contained'}}
  void deref() const { if (!--rc) delete this; }
  mutable unsigned rc{1};
};

template<typename T> struct Ref {
  ~Ref() { derefIfNotNull(m_ptr); }
  T *m_ptr; // expected-note 2 {{holds unmanaged raw-pointer field 'm_ptr'}}
};

// expected-warning@+1 {{could not infer the self-containment of type 'Ref<Parser>'}} FIXME: this should be a view because Parser is a view
template struct Ref<Parser>;

// expected-warning@+1 {{could not infer the self-containment of type 'Ref<Counted>'}} FIXME: should be self-contained.
template struct Ref<Counted>;

// An owned self-reference adds nothing to its own outcome.
// expected-warning@+1 {{could not infer the self-containment of type 'SelfRecursiveNode'}} FIXME: should be self-contained.
struct SelfRecursiveNode {
  void deref() const { if (!--rc) delete this; }
  mutable unsigned rc{1};
  SelfRecursiveNode *next; // expected-note {{holds unmanaged raw-pointer field 'next'}}
  ~SelfRecursiveNode() { derefIfNotNull(next); }
};

// The pointee may be incomplete, unlike a by-value field: nothing is known.
void derefIncomplete(struct Incomplete *) OWNERSHIP_TAKES(1);

// expected-warning@+1 {{could not infer the self-containment of type 'OwnsIncomplete'}}
struct OwnsIncomplete {
  ~OwnsIncomplete() { derefIncomplete(m_ptr); }
  Incomplete *m_ptr; // expected-note {{holds unmanaged raw-pointer field 'm_ptr'}}
};

// An annotation on the forward declaration is trusted even though the
// definition is never seen.
struct SWIFT_SELF_CONTAINED AnnotatedSelfContained;
struct SWIFT_VIEW AnnotatedView;
void derefSC(AnnotatedSelfContained *) OWNERSHIP_TAKES(1);
void derefView(AnnotatedView *) OWNERSHIP_TAKES(1);

// expected-warning@+1 {{could not infer the self-containment of type 'OwnsAnnotatedSelfContained'}} FIXME: should be a view
struct OwnsAnnotatedSelfContained {
  ~OwnsAnnotatedSelfContained() { derefSC(m_ptr); }
  AnnotatedSelfContained *m_ptr; // expected-note {{holds unmanaged raw-pointer field 'm_ptr'}}
};

// expected-warning@+1 {{could not infer the self-containment of type 'OwnsAnnotatedView'}} FIXME: should be a view
struct OwnsAnnotatedView {
  ~OwnsAnnotatedView() { derefView(m_ptr); }
  AnnotatedView *m_ptr; // expected-note {{holds unmanaged raw-pointer field 'm_ptr'}}
};
