// RUN: %clang_cc1 -fsyntax-only -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis -Wlifetime-safety -Wlifetime-safety-experimental-invalidation -Wno-dangling -verify %s

// The ownership_takes application heuristic accepts a `this`-based receiver, not
// only a plain DeclRefExpr variable. This lets a method that consumes `*this` by
// forwarding to an out-of-line `ownership_takes` helper -- including through a
// `const_cast<T&>(*this)` receiver, as WebKit's Node::deref() does -- be inferred
// to consume `*this` itself (so callers observe the invalidation). The effect is
// only observable under TU-wide inference.

struct Widget {};

// (A) `*this` through const_cast (the WebKit Node::deref shape).
struct SelfReleasing {
  void releaseImpl() __attribute__((ownership_takes(res, 1)));
  void release() const { const_cast<SelfReleasing &>(*this).releaseImpl(); }
};
void use(SelfReleasing *);

void UseAfterSelfRelease() {
  SelfReleasing s;
  SelfReleasing *p = &s; // expected-warning {{local variable 's' does not live long enough}}
  p->release();          // expected-note {{local variable 's' is freed here}}
  use(p);                // expected-note {{later used here}}
}

// (B) `this->` (implicit object) receiver.
struct Disposer {
  void disposeImpl() __attribute__((ownership_takes(res, 1)));
  void dispose() { this->disposeImpl(); }
};
void use(Disposer *);

void UseAfterDispose() {
  Disposer d;
  Disposer *p = &d; // expected-warning {{local variable 'd' does not live long enough}}
  p->dispose();     // expected-note {{local variable 'd' is freed here}}
  use(p);           // expected-note {{later used here}}
}
