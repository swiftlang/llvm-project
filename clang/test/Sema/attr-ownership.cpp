// RUN: %clang_cc1 %s -verify -fsyntax-only

class C {
  void *f(int, int)
       __attribute__((ownership_returns(foo, 2)))  // expected-error {{'ownership_returns' attribute index does not match; here it is 2}}
       __attribute__((ownership_returns(foo, 3))); // expected-note {{declared with index 3 here}}
};

struct OwnershipTaker {
  OwnershipTaker(int *) __attribute__((ownership_takes(foo, 1))); // 'this' (the object being constructed) is accepted.
  OwnershipTaker(int *, int) __attribute__((ownership_takes(foo, 2)));
};
