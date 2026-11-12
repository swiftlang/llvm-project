import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil


class TestCase(lldbtest.TestBase):

    @swiftTest
    def test(self):
        """Test that Double is a floating-point scalar, like Float"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        frame = thread.frames[0]
        expected = lldb.eTypeIsScalar | lldb.eTypeIsFloat | lldb.eTypeHasValue
        for name in ["f32", "f64"]:
            flags = frame.FindVariable(name).GetType().GetTypeFlags()
            self.assertEqual(flags & expected, expected, name)
