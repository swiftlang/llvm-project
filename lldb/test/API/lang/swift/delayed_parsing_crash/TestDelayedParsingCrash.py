import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestDelayedParsingCrash(TestBase):

    @swiftTest
    @skipEmbeddedSwiftOnWindows
    def test(self):
        """Test that an expression can declare a typealias to a private class"""
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        # Declarations produce no result, which SBFrame.EvaluateExpression reports
        # as an error.
        self.expect('expr typealias $MyV = V')
