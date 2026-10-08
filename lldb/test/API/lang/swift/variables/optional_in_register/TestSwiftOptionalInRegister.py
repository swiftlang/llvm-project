import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftOptionalInRegister(TestBase):
    def value_in_register(self, thread, name):
        """Fetch a variable from the parent frame, asserting it really is held
        in a register."""
        frame = thread.GetFrameAtIndex(1)
        self.assertIn("caller", frame.GetFunctionName())
        value = frame.FindVariable(name)
        self.assertTrue(value.IsValid(), "no variable named " + name)
        self.assertEqual(
            value.GetLocation(),
            "scalar",
            "%s should be in a register, but its location is '%s'; the "
            "optimizer may have spilled it, which would stop this test from "
            "exercising the bug" % (name, value.GetLocation()),
        )
        return value

    @skipEmbeddedSwiftOnWindows
    @swiftTest
    def test_optional_in_register(self):
        self.build()
        _, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )

        optional = self.value_in_register(thread, "optionalBytes")
        self.assertEqual(optional.GetSummary(), "3 values")
        self.assertEqual([c.GetValue() for c in optional], ["1", "2", "3"])

        plain = self.value_in_register(thread, "plainBytes")
        self.assertEqual(plain.GetSummary(), "4 values")

        process.Continue()
        optional = self.value_in_register(thread, "optionalBytes")
        self.assertEqual(optional.GetSummary(), "nil")
        self.assertEqual(optional.GetNumChildren(), 0)
