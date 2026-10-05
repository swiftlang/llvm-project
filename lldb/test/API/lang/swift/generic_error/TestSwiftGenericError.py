import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftGenericError(TestBase):

    @swiftTest
    @skipEmbeddedSwiftOnWindows
    def test(self):
        """Test that a generic argument bound to an Error shows the concrete error"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        lldbutil.check_variable(self, frame.FindVariable("Pat"), use_dynamic=True,
                                typename="a.MyErr", value="Patatino")
