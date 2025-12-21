// RUN: %clang_cc1 -Wall -Wno-unused -Wno-uninitialized -verify -std=c17 %s
// RUN: %clang_cc1 -Wall -Wno-unused -Wno-uninitialized -verify -std=c23 %s

#define CFI_UNCHECKED_CALLEE __attribute__((cfi_unchecked_callee))

// expected-note@+1 2 {{previous definition is here}}
struct field_attr_test {
  void (CFI_UNCHECKED_CALLEE *func)(void);
};

// expected-error@+1{{redefinition of 'field_attr_test'}}
struct field_attr_test {
  void (CFI_UNCHECKED_CALLEE *func)(void);
};

typedef void (CFI_UNCHECKED_CALLEE func_t)(void);

// expected-error@+1{{redefinition of 'field_attr_test'}}
struct field_attr_test {
  func_t *func;
};
