import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftArrayElement(TestBase):

    @requireNotEmbeddedSwift
    @swiftTest
    def test(self):
        """Test that an array element can be accessed with frame variable"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        lldbutil.check_variable(self, frame.GetValueForVariablePath("patatino[0]"),
                                typename="Swift.Int", value="1")
