import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil


class TestCase(lldbtest.TestBase):
    @swiftTest
    @skipIf(oslist=["windows", "linux"])
    @skipIf(macos_version=["<", "26.0"], asan=True) # rdar://138777205
    def test(self):
        """Test conditions for async step-in."""
        self.build()

        src = lldb.SBFileSpec("main.swift")
        target, _, thread, _ = lldbutil.run_to_source_breakpoint(self, "await f()", src)
        self.assertEqual(
            thread.frame[0].function.mangled,
            self.swiftMangledName("$s1a5entryO4mainyyYaFZ"),
        )

        sym_ctx_list = target.FindFunctions(
            self.swiftMangledName("$s1a5entryO4mainyyYaFZTQ0_")
        )
        self.assertEqual(sym_ctx_list.GetSize(), 1)
        function = sym_ctx_list[0].function
        self.assertIsNotNone(function)
        instructions = list(function.GetInstructions(target))
        self.assertGreater(len(instructions), 0)
        # Expected to be a trampoline that tail calls `swift_task_switch`.
        self.assertIn("swift_task_switch", instructions[-1].GetComment(target))

        # Using the line table, build a set of the non-zero line numbers for
        # this this function - and verify that there is exactly one line.
        lines = {inst.addr.line_entry.line for inst in instructions}
        lines.discard(0)
        self.assertEqual(lines, {3})

        # Required for builds that have debug info.
        # Avoid spurious breakpoints when debug info for the concurrency runtime is available.
        self.runCmd("settings set target.process.thread.step-avoid-libraries libswift_Concurrency.dylib")
        # Same as above, but for embedded swift.
        self.runCmd("settings set target.process.thread.step-avoid-regexp ^(std::|(::)?swift_)")
        thread.StepInto()
        frame = thread.frame[0]
        # Step in from `main` should progress through to `f`.
        self.assertEqual(frame.name, "a.f() async -> Swift.Int")
