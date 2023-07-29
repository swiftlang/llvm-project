import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil


class TestCase(lldbtest.TestBase):
    @swiftTest
    @skipIf(oslist=["windows", "linux"])
    def test(self):
        """Test conditions for async step-in."""
        self.build()

        src = lldb.SBFileSpec("main.swift")
        target, _, thread, _ = lldbutil.run_to_source_breakpoint(self, "await f()", src)
        self.assertEqual(thread.frame[0].function.mangled, "$s1a5entryO4mainyyYaFZ")

        sym_ctx_list = target.FindFunctions("$s1a5entryO4mainyyYaFZTQ0_", lldb.eFunctionNameTypeAuto)
        self.assertEquals(sym_ctx_list.GetSize(), 1)
        symbolContext = sym_ctx_list.GetContextAtIndex(0)
        function = symbolContext.GetFunction()
        if function:
            instructions = list(function.GetInstructions(target))
        else:
            symbol = symbolContext.GetSymbol()
            self.assertIsNotNone(symbol)
            instructions = list(symbol.GetInstructions(target))

        # Expected to be a trampoline that tail calls `swift_task_switch`.
        for inst in reversed(instructions):
            control_flow_kind = inst.GetControlFlowKind(target)
            if control_flow_kind == lldb.eInstructionControlFlowKindOther:
                continue
            self.assertEqual(control_flow_kind, lldb.eInstructionControlFlowKindJump)
            self.assertIn("swift_task_switch", inst.GetComment(target))
            break

        # Using the line table, build a set of the non-zero line numbers for
        # this this function - and verify that there is exactly one line.
        lines = {inst.addr.line_entry.line for inst in instructions}
        lines.remove(0)
        self.assertEqual(lines, {3})

        # Required for builds that have debug info.
        self.runCmd(
            "settings set target.process.thread.step-avoid-libraries libswift_Concurrency.dylib"
        )
        thread.StepInto()
        frame = thread.frame[0]
        # Step in from `main` should progress through to `f`.
        self.assertEqual(frame.name, "a.f() async -> Swift.Int")
