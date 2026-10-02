import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftAnyTypeArray(TestBase):

    @skipEmbeddedSwift
    @swiftTest
    def test(self):
        """Test that an array of Any.Type shows the metatypes it contains"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        patatino = frame.FindVariable("patatino")
        lldbutil.check_variable(self, patatino, typename="Swift.Array<Any.Type>",
                                summary="3 values")
        lldbutil.check_variable(self, patatino.GetChildAtIndex(0), use_dynamic=True,
                                summary="String")
        lldbutil.check_variable(self, patatino.GetChildAtIndex(1), use_dynamic=True,
                                summary="Int")
        lldbutil.check_variable(self, patatino.GetChildAtIndex(2), use_dynamic=True,
                                summary="Float")
