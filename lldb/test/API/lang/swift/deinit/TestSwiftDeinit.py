import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil
import unittest2


class TestSwiftUnknownReference(lldbtest.TestBase):

    mydir = lldbtest.TestBase.compute_mydir(__file__)
    
    @swiftTest
    def test_swift_early_deinit(self):
        self.build()
        target, process, thread, bkpt = lldbutil.run_to_source_breakpoint(
            self, 'break here', lldb.SBFileSpec('main.swift'))

        self.expect("frame variable -d no-run -- b", substrs=["(a.K) b = <deinitialized>"])
        self.expect("expr b", error=True, substrs=["variable b has been deinitialized"])
