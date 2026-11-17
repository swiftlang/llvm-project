// Test the check that computing the end of a struct with a flexible array
// member doesn't overflow the address space.
//
// In soft trap mode execution continues after the failed check and the
// pointer to the struct (which is at the end of the address space) is
// dereferenced, which crashes. So this test can't be run in that mode.
// UNSUPPORTED: soft-traps
//
// RUN: %clang_bsafe %s -o %t
// RUN: %expect-no-trap %t
// RUN: %expect-trap --verify-prefix=cast %s %t 1
#include <ptrcheck.h>
#include <stdint.h>
#include <stdio.h>

struct flexible {
  int count;
  int elems[__counted_by(count)];
};

// `noinline` here is used to prevent the check from being optimized using the
// values passed by `main`.

// cast-merged{to_single}
struct flexible *__single __attribute__((noinline))
to_single(struct flexible *__bidi_indexable flex) {
  // cast@+1{converting 'struct flexible *__bidi_indexable' to 'struct flexible *__single' where pointer 'flex' overflows the address space}
  return flex;
}

int main(int argc, const char **__counted_by(argc) argv) {
  if (argc == 1) {
    // The struct doesn't overflow: the check is performed but doesn't fail.
    // The storage for a `struct flexible` with room for 2 elements.
    int storage[3] = {0, 10, 20};
    struct flexible *__bidi_indexable flex = __unsafe_forge_bidi_indexable(
        struct flexible *, storage, sizeof(storage));
    flex->count = 2;
    printf("%d\n", to_single(flex)->count);
    return 0;
  }
  // The struct is at the end of the address space.
  struct flexible *__single result = to_single(__unsafe_forge_bidi_indexable(
      struct flexible *, UINTPTR_MAX - sizeof(struct flexible) + 1,
      sizeof(struct flexible)));
  printf("%d\n", result->count);
  return 0;
}
