"""
Test that Int128 is printed signed, across the whole range.
"""

import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftInt128(TestBase):
    @swiftTest
    @skipEmbeddedSwiftOnWindows
    def test(self):
        """Test that Int128 values are printed as signed decimal."""
        self.build()

        target, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )

        frame = thread.frames[0]
        self.assertTrue(frame, "Frame 0 is valid.")

        lldbutil.check_variable(
            self, frame.FindVariable("negative"), False, value="-42"
        )
        lldbutil.check_variable(
            self, frame.FindVariable("positive"), False, value="42"
        )

        # The ends of the range, where the sign bit alone decides the value.
        lldbutil.check_variable(
            self,
            frame.FindVariable("mostNegative"),
            False,
            value="-170141183460469231731687303715884105728",
        )
        lldbutil.check_variable(
            self,
            frame.FindVariable("mostPositive"),
            False,
            value="170141183460469231731687303715884105727",
        )
