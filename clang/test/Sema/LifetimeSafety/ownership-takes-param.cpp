// RUN: %clang_cc1 -fsyntax-only -Wlifetime-safety -Wlifetime-safety-experimental-invalidation -Wno-dangling -verify=expected %s
// RUN: %clang_cc1 -fsyntax-only -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis -Wlifetime-safety -Wlifetime-safety-experimental-invalidation -Wno-dangling -verify=expected,tu %s

// Verifies that `ownership_takes` naming a regular parameter (not the implicit
// object) invalidates the corresponding argument at the call site -- both when
// written explicitly (on a free function or a member's explicit parameter) and
// when inferred by the lifetime-safety analysis for a function that deletes a
// parameter.

struct Widget {};
void use(Widget *);

// (A) Explicit ownership_takes on a free-function parameter (source index 1).
void destroy_widget(Widget *w) __attribute__((ownership_takes(res, 1)));

void ExplicitFreeFunction() {
  Widget x;
  Widget *p = &x;    // expected-warning {{local variable 'x' does not live long enough}}
  destroy_widget(p); // expected-note {{local variable 'x' is freed here}}
  use(p);            // expected-note {{later used here}}
}

// (B) Explicit ownership_takes on a member's explicit parameter. Source index 2
// because index 1 is the implicit object parameter.
struct Sink {
  void take(Widget *w) __attribute__((ownership_takes(res, 2)));
};

void ExplicitMemberParameter(Sink &s) {
  Widget x;
  Widget *p = &x; // expected-warning {{local variable 'x' does not live long enough}}
  s.take(p);      // expected-note {{local variable 'x' is freed here}}
  use(p);         // expected-note {{later used here}}
}

// Non-taken parameters do not invalidate their argument: no false positive.
void passthrough(Widget *w);

void NonTakingCallIsOk() {
  Widget x;
  Widget *p = &x;
  passthrough(p);
  use(p); // ok
}

// (C) Inferred: a free function that deletes its parameter is marked
// ownership_takes by the analysis (TU inference only).
void sink(Widget *w) { delete w; }

void InferredDeletedParameter() {
  Widget x;
  Widget *p = &x; // tu-warning {{local variable 'x' does not live long enough}}
  sink(p);        // tu-note {{local variable 'x' is freed here}}
  use(p);         // tu-note {{later used here}}
}

// (D) Explicit ownership_takes on a constructor's explicit parameter. A
// constructor call has no implicit object argument in the call's argument list,
// but the source index still counts the object being constructed (index 1), so
// the first explicit parameter is index 2.
struct Consumer {
  Consumer(Widget *w) __attribute__((ownership_takes(res, 2)));
};

void ConstructorParameter() {
  Widget x;
  Widget *p = &x;  // expected-warning {{local variable 'x' does not live long enough}}
  Consumer c(p);   // expected-note {{local variable 'x' is freed here}}
  use(p);          // expected-note {{later used here}}
}

// (E) Inferred: a member function that deletes its explicit parameter is marked
// ownership_takes on that parameter (source index 2), exercising the
// implicit-object index adjustment.
struct MemberSink {
  void consume(Widget *w) { delete w; }
};

void InferredMemberParameter(MemberSink &s) {
  Widget x;
  Widget *p = &x;  // tu-warning {{local variable 'x' does not live long enough}}
  s.consume(p);    // tu-note {{local variable 'x' is freed here}}
  use(p);          // tu-note {{later used here}}
}

// (F) `ownership_holds` (as opposed to `ownership_takes`) does NOT consume its
// argument, so the pointer stays valid: no invalidation (exercises the
// `!isTakes()` skip in handleOwnershipTakesCall).
void hold_widget(Widget *w) __attribute__((ownership_holds(res, 1)));

void HoldsDoesNotInvalidate() {
  Widget x;
  Widget *p = &x;
  hold_widget(p);
  use(p); // ok
}

// (G) `ownership_takes` naming the implicit object (index 1) on a CONSTRUCTOR:
// the object being constructed is not a call argument, so there is nothing to
// invalidate -- the attribute is silently skipped (no crash, no false positive).
struct SelfConsumingCtor {
  SelfConsumingCtor() __attribute__((ownership_takes(res, 1)));
};

void ConstructorTakesObjectIsNoop() {
  SelfConsumingCtor c;
  (void)c; // no diagnostic
}
