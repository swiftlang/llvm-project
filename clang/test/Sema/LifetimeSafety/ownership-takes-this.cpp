// RUN: %clang_cc1 -fsyntax-only -Wlifetime-safety -Wlifetime-safety-experimental-invalidation -Wno-dangling -verify=expected %s
// RUN: %clang_cc1 -fsyntax-only -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis -Wlifetime-safety -Wlifetime-safety-experimental-invalidation -Wno-dangling -verify=expected,tu %s

// Verifies that `ownership_takes` naming the implicit object parameter ("this",
// source index 1) invalidates the receiver at the call site -- both when
// written explicitly and when inferred by the lifetime-safety analysis for a
// method that deletes `this` (a "delete this" style release method).

struct Widget {
  // `ownership_takes(res, 1)` marks the method as deleting/consuming `*this`.
  void destroy() __attribute__((ownership_takes(res, 1)));
};

struct Node {
  // The analysis infers `ownership_takes(this)` here (TU inference only), so
  // calling release() invalidates the receiver.
  void release() { delete this; }
};

void use(Widget *);
void use(Node *);

// The explicit attribute is honored in both function-local and TU-wide modes.
void UseAfterExplicitDestroy() {
  Widget w;
  Widget *p = &w; // expected-warning {{local variable 'w' does not live long enough}}
  p->destroy();   // expected-note {{local variable 'w' is freed here}}
  use(p);         // expected-note {{later used here}}
}

// Re-establishing the pointer after invalidation is fine: no false positive.
void ReassignAfterDestroyIsOk() {
  Widget w1, w2;
  Widget *p = &w1;
  p->destroy();
  p = &w2;
  use(p); // ok
}

void UseAfterInferredRelease() {
  Node n;
  Node *p = &n; // tu-warning {{local variable 'n' does not live long enough}}
  p->release(); // tu-note {{local variable 'n' is freed here}}
  use(p);       // tu-note {{later used here}}
}
