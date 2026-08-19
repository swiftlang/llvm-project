"""
Test that Float16 reports a scalar value.
"""

import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftFloat16(TestBase):
    @swiftTest
    @skipEmbeddedSwiftOnWindows
    # Float16 is unavailable in the stdlib on x86_64 macOS.
    @skipIf(oslist=["macosx"], archs=["x86_64"])
    def test(self):
        """Test that Float16 has a value, like Float and Double do."""
        self.build()

        target, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )

        frame = thread.frames[0]
        self.assertTrue(frame, "Frame 0 is valid.")

        # The controls: both report a value rather than only a _value child.
        lldbutil.check_variable(self, frame.FindVariable("f32"), False, value="2.5")
        lldbutil.check_variable(self, frame.FindVariable("f64"), False, value="3.5")

        lldbutil.check_variable(self, frame.FindVariable("f16"), False, value="1.5")
