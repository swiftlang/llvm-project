// Test the address space overflow check performed when dereferencing a
// `__bidi_indexable` pointer.
//
// In soft trap mode execution continues after the failed check and the
// pointer (which is at the end of the address space) is dereferenced, which
// crashes. So this test can't be run in that mode.
// UNSUPPORTED: soft-traps
//
// RUN: %clang_bsafe %s -o %t
// RUN: %expect-no-trap %t
// RUN: %expect-trap --verify-prefix=overflow %s %t 1
#include <ptrcheck.h>
#include <stdint.h>
#include <stdio.h>

// `noinline` here is used to prevent the check from being optimized using the
// values passed by `main`.

// overflow-merged{load}
int __attribute__((noinline)) load(int *__bidi_indexable p) {
  // overflow@+1{dereferencing overflows address space in '*p'}
  return *p;
}

int main(int argc, const char **__counted_by(argc) argv) {
  if (argc == 1) {
    // In bounds: the checks are performed but don't fail.
    int storage[1] = {1};
    printf("%d\n", load(storage));
    return 0;
  }
  // Computing the pointer one past the element overflows.
  printf("%d\n", load(__unsafe_forge_bidi_indexable(
                     int *, UINTPTR_MAX - sizeof(int) + 1, sizeof(int))));
  return 0;
}
