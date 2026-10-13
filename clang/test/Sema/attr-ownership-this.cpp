// RUN: %clang_cc1 %s -verify -fsyntax-only

// The implicit C++ 'this' parameter (index 1 on an instance method) may be
// marked with ownership_takes/ownership_holds: it denotes a method that frees
// the object itself (e.g. a 'delete this' style release method).

struct S {
  // 'this' is index 1; the attribute is accepted.
  void release() __attribute__((ownership_takes(pool, 1)));

  // 'this' is index 1, 'p' is index 2. Both are pointers and accepted.
  void releaseBoth(int *p) __attribute__((ownership_takes(pool, 1, 2)));

  // Marking only an explicit parameter still works.
  void releaseArg(int *p) __attribute__((ownership_takes(pool, 2)));

  // ownership_holds on 'this' is also valid.
  void hold() __attribute__((ownership_holds(pool, 1)));

  // ownership_returns requires an integer argument, but 'this' is a pointer.
  int *allocate() __attribute__((ownership_returns(pool, 1))); // expected-error {{'ownership_returns' attribute only applies to integer arguments}}
};

// A static member function has no implicit 'this'; index 1 is the first
// explicit parameter.
struct T {
  static void release(int *p) __attribute__((ownership_takes(pool, 1)));
};

// Free functions are unaffected: index 1 is the first explicit parameter.
void freeFn(int *p) __attribute__((ownership_takes(pool, 1)));
