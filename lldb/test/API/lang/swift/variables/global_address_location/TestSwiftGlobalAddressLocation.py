import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil

class TestCase(lldbtest.TestBase):
    @skipEmbeddedSwiftOnWindows
    @swiftTest
    def test(self):
        """A variable holding a global's address must have a debug location"""
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, 'break here', lldb.SBFileSpec('main.swift'))

        # A dbg_value naming a global has no DWARF location at -Onone, so a
        # variable whose storage is a global's address is only readable if it
        # gets a stack slot.
        self.expect_var_path("cfn", type="(Int) -> Int")
        self.expect_var_path("thinfn", type="(Int) -> Int")
