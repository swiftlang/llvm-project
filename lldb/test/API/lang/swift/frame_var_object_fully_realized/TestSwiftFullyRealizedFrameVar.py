import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftFullyRealizedFrameVar(TestBase):

    @swiftTest
    @skipEmbeddedSwiftOnWindows
    def test(self):
        """Test that frame variable -O shows a generic argument as its bound type"""
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        # -O prints the object description, which has no SBValue equivalent.
        self.expect('frame variable -d run -O -- a', substrs=['(UInt8) a = 97'])
