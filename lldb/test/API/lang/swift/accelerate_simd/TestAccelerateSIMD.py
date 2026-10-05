import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestAccelerateSIMD(TestBase):

    @requireNotEmbeddedSwift
    @swiftTest
    def test(self):
        """Test that SIMD vectors are summarized as tuples of their elements"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        lldbutil.check_variable(self, frame.FindVariable("d4"),
                                summary="(1.5, 2, 3, 4)")
        lldbutil.check_variable(self, frame.FindVariable("patatino"),
                                summary="(1, 2, 3, 4)")
        lldbutil.check_variable(self, frame.FindVariable("tinky"),
                                summary="(12, 24)")
