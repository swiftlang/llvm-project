// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -x objective-c -fexperimental-bounds-safety-objc -verify %s

#include <ptrcheck.h>

// expected-no-diagnostics

__attribute__((objc_root_class))
@interface Accumulator
- (int *__counted_by(n))take:(int)n buf:(int *__counted_by(n))buf;
- (int *__counted_by(n))takeNull:(int)n;
- (void *__sized_by(n))takeSized:(unsigned long)n buf:(void *__sized_by(n))buf;
- (int)plain;
@end

@implementation Accumulator
// A bounds-attributed return type on a method: this is what used to assert.
- (int *__counted_by(n))take:(int)n buf:(int *__counted_by(n))buf {
  return buf;
}

- (int *__counted_by(n))takeNull:(int)n {
  return 0;
}

- (void *__sized_by(n))takeSized:(unsigned long)n buf:(void *__sized_by(n))buf {
  return buf;
}

// A method without a bounds-attributed return type must keep working.
- (int)plain {
  return 42;
}
@end
