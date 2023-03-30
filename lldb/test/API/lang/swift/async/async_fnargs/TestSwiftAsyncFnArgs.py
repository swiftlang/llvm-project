import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil
import unittest2


class TestSwiftAsyncFnArgs(lldbtest.TestBase):

    mydir = lldbtest.TestBase.compute_mydir(__file__)

    @swiftTest
    @skipIf(oslist=['windows', 'linux'])
    def test(self):
        """Test function arguments in async functions"""
        self.build()
        self.do_test()

    @swiftTest
    @skipIf(oslist=['windows', 'linux'])
    def test_backdeploy(self):
        """Test function arguments in async functions"""
        self.build(dictionary={
            'TARGET_SWIFTFLAGS': f'-target {self.getArchitecture()}-apple-macos11'
        })
        self.do_test()

    def do_test(self):
        src = lldb.SBFileSpec('main.swift')
        target, process, thread, bkpt = lldbutil.run_to_source_breakpoint(
            self, 'Set breakpoint here', src)

        self.expect("frame var -d run-target -- msg", substrs=['"world"'])

        # Continue into the second coroutine funclet.
        bkpt2 = target.BreakpointCreateBySourceRegex("And also here", src, None)
        self.assertGreater(bkpt2.GetNumLocations(), 0)
        process.Continue()
        self.assertEqual(
             len(lldbutil.get_threads_stopped_at_breakpoint(process, bkpt2)), 1)

        self.expect("frame var -d run-target -- msg", substrs=['"world"'])
