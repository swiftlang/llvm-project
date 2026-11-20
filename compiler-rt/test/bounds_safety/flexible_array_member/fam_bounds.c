// Test the checks that the flexible array member of a struct starts below the
// upper bound of the pointer to the struct and that the pointer is above its
// lower bound.
//
// In soft trap mode execution continues after the failed check and other
// checks fail too, so multiple soft traps happen, which isn't supported by
// `expect_trap.py`.
// UNSUPPORTED: soft-traps
//
// RUN: %clang_bsafe %s -o %t
// RUN: %expect-no-trap %t
// RUN: %expect-trap --verify-prefix=upper %s %t 1
// RUN: %expect-trap --verify-prefix=lower %s %t 1 2
#include <ptrcheck.h>
#include <stdio.h>

struct flexible {
  int count;
  int elems[__counted_by(count)];
};

// upper-merged{to_single}
// lower-merged{to_single}
struct flexible *__single __attribute__((noinline))
to_single(struct flexible *__bidi_indexable flex) {
  return flex;
}

int main(int argc, const char **__counted_by(argc) argv) {
  int storage[3] = {0, 10, 20};
  if (argc == 1) {
    struct flexible *__bidi_indexable flex = __unsafe_forge_bidi_indexable(
        struct flexible *, storage, sizeof(storage));
    flex->count = 2;
    printf("%d\n", to_single(flex)->count);
    return 0;
  }
  if (argc == 2) {
    // The bounds are smaller than the fixed part of the struct.
    struct flexible *__bidi_indexable flex =
        __unsafe_forge_bidi_indexable(struct flexible *, storage, 2);
    // FIXME: The `flexible array member 'elems' starts above the upper bound of
    // pointer 'flex'` trap reason isn't used here because a prior
    // `Dereferencing above bounds` check fires first (rdar://104875616). That
    // check has no source location so the trap is reported at the call to
    // `to_single`.
    // upper@+1{Dereferencing above bounds}
    printf("%d\n", to_single(flex)->count);
  } else {
    // The pointer is below its lower bound.
    int *__bidi_indexable ptr =
        __unsafe_forge_bidi_indexable(int *, &storage[1], 2 * sizeof(int));
    struct flexible *__bidi_indexable flex = (struct flexible *)(ptr - 1);
    // FIXME: The `pointer 'flex' is below its lower bound` trap reason isn't
    // used here because a prior `Dereferencing below bounds` check fires first
    // (rdar://104875616). That check has no source location so the trap is
    // reported at the call to `to_single`.
    // lower@+1{Dereferencing below bounds}
    printf("%d\n", to_single(flex)->count);
  }
  return 0;
}
