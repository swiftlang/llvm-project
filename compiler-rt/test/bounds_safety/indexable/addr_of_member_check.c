// Test the bounds checks performed when taking the address of a struct member
// through an `__indexable` pointer. An `__indexable` pointer doesn't have a
// lower bound so there is no lower bound check.
//
// RUN: %clang_bsafe %s -o %t
// RUN: %expect-no-trap %t
// RUN: %expect-trap --verify-prefix=above %s %t 1
// RUN: %expect-trap --verify-prefix=overflow %s %t 1 2
#include "soft_trap_runtime_impl.h"
#include <ptrcheck.h>
#include <stdint.h>
#include <stdio.h>

struct S {
  int a;
  int b;
};

// `noinline` here is used to prevent the checks from being optimized using the
// values passed by `main`. It also prevents inlining which can indirectly lead
// to LLVM figuring out UB is happening during soft traps and emit a hard trap
// after the soft trap call. This is a workaround for rdar://183581715

// above-merged{addr_of_b}
// overflow-merged{addr_of_b}
int *__bidi_indexable __attribute__((noinline))
addr_of_b(struct S *__indexable p) {
  // above@+2{taking address of struct member above upper bound in '&p->b'}
  // overflow@+1{taking address of struct member overflows address space in '&p->b'}
  return &p->b;
}

int main(int argc, const char **__counted_by(argc) argv) {
  struct S storage[3] = {{1, 2}, {3, 4}, {5, 6}};
  // Only `storage[1]` is within the bounds of `p`. In soft trap mode
  // execution continues after the failed check, so the resulting pointers
  // aren't dereferenced in the cases where the check fails.
  struct S *__indexable p =
      __unsafe_forge_bidi_indexable(struct S *, &storage[1], sizeof(struct S));
  // A struct at the end of the address space.
  struct S *__indexable end = __unsafe_forge_bidi_indexable(
      struct S *, UINTPTR_MAX - sizeof(struct S) + 1, sizeof(struct S));
  switch (argc) {
  case 1:
    // In bounds: the checks are performed but don't fail.
    printf("%d\n", *addr_of_b(p));
    break;
  case 2:
    addr_of_b(p + 1);
    break;
  default:
    addr_of_b(end);
    break;
  }
  printf("done\n");
  return 0;
}
