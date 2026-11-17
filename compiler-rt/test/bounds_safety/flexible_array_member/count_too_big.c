// Test the check that the count of a flexible array member doesn't exceed the
// bounds of the pointer to the struct.
//
// RUN: %clang_bsafe %s -o %t
// RUN: %expect-no-trap %t
// RUN: %expect-trap --verify-prefix=cast %s %t 1
// RUN: %expect-trap --verify-prefix=deref %s %t 1 2
// RUN: %expect-trap --verify-prefix=assign %s %t 1 2 3
#include "soft_trap_runtime_impl.h"
#include <ptrcheck.h>
#include <stdio.h>

struct flexible {
  int count;
  int elems[__counted_by(count)];
};

// `noinline` here is used to prevent the checks from being optimized using the
// values passed by `main`. It also prevents inlining which can indirectly lead
// to LLVM figuring out UB is happening during soft traps and emit a hard trap
// after the soft trap call. This is a workaround for rdar://183581715

// cast-merged{to_single}
struct flexible *__single __attribute__((noinline))
to_single(struct flexible *__bidi_indexable flex) {
  // cast@+1{converting 'struct flexible *__bidi_indexable' to 'struct flexible *__single' where count 'flex->count' for flexible array member 'elems' exceeds the bounds of pointer 'flex'}
  return flex;
}

// deref-merged{first_elem}
int __attribute__((noinline))
first_elem(struct flexible *__bidi_indexable flex) {
  // deref@+1{dereferencing 'struct flexible *__bidi_indexable' where count 'flex->count' for flexible array member 'elems' exceeds the bounds of pointer 'flex'}
  return (*flex).elems[0];
}

// assign-merged{assign}
void __attribute__((noinline)) assign(struct flexible *__single f,
                                      void *__bidi_indexable buf, int n) {
  // assign@+1{converting 'struct flexible *__bidi_indexable' to 'struct flexible *__single' where count 'n' for flexible array member 'elems' exceeds the bounds of pointer 'buf'}
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
    // The count fits: the checks are performed but don't fail.
    printf("%d %d\n", to_single(flex)->count, first_elem(flex));
    assign(0, storage, 2);
    return 0;
  }
  // The count is too big. In soft trap mode execution continues after the
  // failed check but the elements past the bounds aren't accessed.
  flex->count = 3;
  switch (argc) {
  case 2:
    printf("%d\n", to_single(flex)->count);
    break;
  case 3:
    printf("%d\n", first_elem(flex));
    break;
  default:
    assign(0, storage, 3);
    break;
  }
  return 0;
}
