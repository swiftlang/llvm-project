// RUN: %clang_cc1 -fsyntax-only -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis -Wlifetime-safety -Wno-dangling -verify %s

// A function that only MUTATES a container parameter (e.g. push_back, which
// invalidates interior iterators/pointers but does not consume the container
// itself) must NOT be inferred as taking ownership of that parameter. Otherwise
// callers would spuriously treat the whole container as invalidated across the
// call. Only destroying/consuming invalidations (delete, destructor, destroy_at,
// an ownership_takes callee) justify inferring ownership_takes.

#include "Inputs/lifetime-analysis.h"

void mutate(std::vector<int> &v) { v.push_back(1); }

void MutatingParamIsNotTaken(std::vector<int> &v) {
  int *p = &v[0];
  mutate(v);
  (void)*p; // 'mutate' does not consume 'v'; 'p' is not invalidated by the call.
}

// expected-no-diagnostics
