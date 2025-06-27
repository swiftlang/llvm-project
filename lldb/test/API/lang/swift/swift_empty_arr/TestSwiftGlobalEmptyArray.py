import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftGlobalEmptyArray(lldbtest.TestBase):
    @swiftTest
    def test(self):
        """Test that printing a global swift array of type SwiftEmptyArrayStorage uses the correct data formatter"""

        self.build()
        filespec = lldb.SBFileSpec("main.swift")
        target, process, thread, breakpoint1 = lldbutil.run_to_source_breakpoint(
            self, "break here", filespec
        )
        self.expect("p x", substrs=["([a.P]) 0 values {}"])
