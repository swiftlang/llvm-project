import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil


class TestCase(lldbtest.TestBase):

    @skipEmbeddedSwift
    @swiftTest
    @skipIf(oslist=["windows"])
    def test(self):
        """Test line breakpoints in the funclets of async top-level code"""
        self.build()

        source_file = lldb.SBFileSpec("main.swift")
        line = lldbtest.line_number("main.swift", "// break here")
        _, _, thread, _ = lldbutil.run_to_line_breakpoint(self, source_file, line)

        function = thread.frames[0].GetFunction()
        self.assertIn("async_Main", function.GetName())
        self.assertTrue(function.GetType().IsFunctionType())
