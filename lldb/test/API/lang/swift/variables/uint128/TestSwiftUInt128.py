"""
Test that UInt128 is printed unsigned.
"""

import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftUInt128(TestBase):
    @swiftTest
    @skipEmbeddedSwiftOnWindows
    def test(self):
        """Test that a UInt128 with its high bit set is not printed signed."""
        self.build()

        target, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )

        frame = thread.frames[0]
        self.assertTrue(frame, "Frame 0 is valid.")

        # The controls: a UInt128 with the high bit clear, and a real Int128.
        lldbutil.check_variable(self, frame.FindVariable("small"), False, value="42")
        lldbutil.check_variable(self, frame.FindVariable("signed"), False, value="-42")

        lldbutil.check_variable(
            self,
            frame.FindVariable("big"),
            False,
            value="320500633375363362511929115231785247216",
        )
