"""
Test that a variable whose type is an unbound associated type can be printed.
"""

import lldb
import lldbsuite.test.lldbutil as lldbutil
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *


class TestCase(TestBase):
    NO_DEBUG_INFO_TESTCASE = True

    # Embedded Swift fully specializes the closure, so its parameter type is
    # never a dependent member type there.
    @skipEmbeddedSwift
    @swiftTest
    def test(self):
        """The parameter of the closure in printAll() has the dependent member
        type `τ_0_0.Element`. Usually LLDB resolves that archetype to the
        concrete type via the runtime, but in the closure's epilogue the
        argument's storage is already gone, so the resolution fails and LLDB
        has to print the unbound archetype itself."""
        self.build()
        target, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))

        # Put a breakpoint on every instruction of the closure so that every
        # point of its epilogue is visited.
        instructions = thread.GetFrameAtIndex(0).GetFunction().GetInstructions(
            target)
        self.assertTrue(instructions.GetSize() > 0)
        target.DeleteAllBreakpoints()
        for i in range(instructions.GetSize()):
            target.BreakpointCreateBySBAddress(
                instructions.GetInstructionAtIndex(i).GetAddress())

        # Printing the closure's argument must never crash, and once the
        # archetype can no longer be bound its raw value is printed instead.
        saw_unbound_archetype = False
        while process.GetState() == lldb.eStateStopped:
            frame = process.GetSelectedThread().GetFrameAtIndex(0)
            argument = frame.FindVariable("$0")
            self.assertTrue(argument.IsValid())
            if argument.GetTypeName() != "a.SpecificClass":
                self.assertEqual(argument.GetTypeName(), "τ_0_0.Element")
                self.assertIsNotNone(argument.GetValue())
                self.assertTrue(argument.GetValue().startswith("0x"))
                saw_unbound_archetype = True
            process.Continue()

        self.assertTrue(saw_unbound_archetype,
                        "never saw the unbound associated type")
