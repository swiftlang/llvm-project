// RUN: %clang_cc1 -fsyntax-only \
// RUN:     -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis \
// RUN:     -Wlifetime-safety-experimental-field-origin-escape \
// RUN:     %s -Wownership-inference 2>&1 | FileCheck %s

// Regression test: a SELF_CONTAINED_IF container decomposes to its element
// type, so two such containers that refer to each other's enclosing record
// (Ping holds Holder<Pong>, Pong holds Holder<Ping>) make inference mutually
// recursive. Because the verdict is memoized only on completion, this used to
// recurse until the stack overflowed and clang crashed (SIGSEGV). The
// in-progress guard breaks the cycle by treating a record already under
// analysis as 'unknown'. This test passes if clang *terminates*; the verdicts
// are conservative (everything in the cycle becomes 'failed to infer').
#define SELF_CONTAINED_IF(...) \
  __attribute__((swift_attr("escapable_if:" #__VA_ARGS__)))

template <class T> struct SELF_CONTAINED_IF(T) Holder {
  void *storage;
  ~Holder();
};

struct Pong;
struct Ping { Holder<Pong> m; };
struct Pong { Holder<Ping> m; };

Holder<Ping> hp;

// CHECK: type 'Ping' failed to infer
// CHECK: type 'Pong' failed to infer
