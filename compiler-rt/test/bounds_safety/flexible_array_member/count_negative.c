// Test the check that the count of a flexible array member isn't negative.
//
// In soft trap mode execution continues after the failed check and the
// negative count also fails the check that the count doesn't exceed the
// bounds of the pointer to the struct, so two soft traps happen, which isn't
// supported by `expect_trap.py`.
// UNSUPPORTED: soft-traps
//
// RUN: %clang_bsafe %s -o %t
// RUN: %expect-no-trap %t
// RUN: %expect-trap --verify-prefix=cast %s %t 1
// RUN: %expect-trap --verify-prefix=deref %s %t 1 2
// RUN: %expect-trap --verify-prefix=assign %s %t 1 2 3
#include <ptrcheck.h>
#include <stdio.h>

struct flexible {
  int count;
  int elems[__counted_by(count)];
};

// `noinline` here is used to prevent the checks from being optimized using the
// values passed by `main`.

// cast-merged{to_single}
struct flexible *__single __attribute__((noinline))
to_single(struct flexible *__bidi_indexable flex) {
  // cast@+1{converting 'struct flexible *__bidi_indexable' to 'struct flexible *__single' where count 'flex->count' for flexible array member 'elems' is negative}
  return flex;
}

// deref-merged{first_elem}
int __attribute__((noinline))
first_elem(struct flexible *__bidi_indexable flex) {
  // deref@+1{dereferencing 'struct flexible *__bidi_indexable' where count 'flex->count' for flexible array member 'elems' is negative}
  return (*flex).elems[0];
}

// assign-merged{assign}
void __attribute__((noinline)) assign(struct flexible *__single f,
                                      void *__bidi_indexable buf, int n) {
  // assign@+1{converting 'struct flexible *__bidi_indexable' to 'struct flexible *__single' where count 'n' for flexible array member 'elems' is negative}
  f = buf;
  f->count = n;
}

int main(int argc, const char **__counted_by(argc) argv) {
  // The storage for a `struct flexible` with room for 2 elements.
  int storage[3] = {0, 10, 20};
  struct flexible *__bidi_indexable flex = __unsafe_forge_bidi_indexable(
      struct flexible *, storage, sizeof(storage));
  if (argc == 1) {
    flex->count = 2;
    // The count isn't negative: the checks are performed but don't fail.
    printf("%d %d\n", to_single(flex)->count, first_elem(flex));
    assign(0, storage, 2);
    return 0;
  }
  // The count is negative.
  flex->count = -1;
  switch (argc) {
  case 2:
    printf("%d\n", to_single(flex)->count);
    break;
  case 3:
    printf("%d\n", first_elem(flex));
    break;
  default:
    assign(0, storage, -1);
    break;
  }
  return 0;
}
