// RUN: %clang_analyze_cc1 -std=c++2b -verify \
// RUN:     -analyzer-checker=core,unix.Malloc \
// RUN:     -analyzer-config unix.DynamicMemoryModeling:Optimistic=true \
// RUN:     -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis \
// RUN:     -Wlifetime-safety -Wno-dangling %s

// The lifetime-safety inference synthesizes an *implicit* ownership_takes on
// `sink` (it deletes its parameter). MallocChecker consumes real ownership_takes
// attributes (under Optimistic modeling) to model deallocation, but it must
// ignore *implicit* ones -- they are inferred internally, not authored by the
// user -- so it must not model `sink(&local)` as freeing stack memory.

struct Widget {};

void sink(Widget *w) { delete w; }

void test() {
  Widget local;
  sink(&local); // no 'address of stack memory' diagnostic from MallocChecker
}

// expected-no-diagnostics
