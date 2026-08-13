import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftExpressionConstantMaterialization(TestBase):
    NO_DEBUG_INFO_TESTCASE = True

    @skipEmbeddedSwiftOnWindows
    @swiftTest
    def test(self):
        """Test constants can be materialized"""
        self.build()
        target, process, thread, bkpt = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )

        self.expect("expression null", substrs=["nil"])
        # A materialization failure can have ripple effects on unrelated expressions.
        self.expect("expression i", substrs=["23"])
