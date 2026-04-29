#if TEST_SOFT_TRAP
#include <bounds_safety_soft_traps.h>
#include <stdio.h>

#if __CLANG_BOUNDS_SAFETY_SOFT_TRAP_API_VERSION > 0
#error API changed
#endif

static unsigned trap_counter = 0;

// If this is inlined LLDB won't be able to set a breakpoint (or use the bounds
// safety instrumentation plugin which internally relies on setting a
// breakpoint) on the soft trap. The `expect_trap.py` script relies on being
// able to observe the soft trap, so we have to use the `noinline` attribute to
// avoid this.
__attribute__((noinline))
void __bounds_safety_soft_trap(void) {
  // print a simple message that `expect_trap.py` can look for
  fprintf(stderr, "***HIT BOUNDS SAFETY SOFT TRAP***: %d\n", trap_counter);
  ++trap_counter;
}

#endif
