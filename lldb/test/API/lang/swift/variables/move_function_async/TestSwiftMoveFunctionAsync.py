# TestSwiftMoveFunctionAsync.py
#
# This source file is part of the Swift.org open source project
#
# Copyright (c) 2014 - 2016 Apple Inc. and the Swift project authors
# Licensed under Apache License v2.0 with Runtime Library Exception
#
# See https://swift.org/LICENSE.txt for license information
# See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
#
# ------------------------------------------------------------------------------
"""
Check that we properly show variables at various points of the CFG while
stepping with the move function.
"""
import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil
import os
import sys
import unittest2

def stderr_print(line):
    sys.stderr.write(line + "\n")

class TestSwiftMoveFunctionAsyncType(TestBase):

    mydir = TestBase.compute_mydir(__file__)

    @swiftTest
    def test_swift_move_function_async(self):
        """Check that we properly show variables at various points of the CFG while
        stepping with the move function.
        """
        self.build()

        self.exec_artifact = self.getBuildArtifact(self.exec_name)
        self.target = self.dbg.CreateTarget(self.exec_artifact)
        self.assertTrue(self.target, VALID_TARGET)

        self.do_setup_breakpoints()

        self.process = self.target.LaunchSimple(None, None, os.getcwd())
        threads = lldbutil.get_threads_stopped_at_breakpoint(
            self.process, self.breakpoints[0])
        self.assertEqual(len(threads), 1)
        self.thread = threads[0]

        self.do_check_copyable_value_test()

    def setUp(self):
        TestBase.setUp(self)
        self.main_source = "main.swift"
        self.main_source_spec = lldb.SBFileSpec(self.main_source)
        self.exec_name = "a.out"

    def add_breakpoints(self, name, num_breakpoints):
        pattern = 'Set breakpoint {} here {}'
        for i in range(num_breakpoints):
            pat = pattern.format(name, i+1)
            brk = self.target.BreakpointCreateBySourceRegex(
                pat, self.main_source_spec)
            self.assertGreater(brk.GetNumLocations(), 0, VALID_BREAKPOINT)
            yield brk

    def get_var(self, name):
        frame = self.thread.frames[0]
        self.assertTrue(frame.IsValid(), "Couldn't get a frame.")
        return frame.FindVariable(name)

    def do_setup_breakpoints(self):
        self.breakpoints = []

        self.breakpoints.extend(
            self.add_breakpoints('copyableValueTest', 5))

    def do_check_copyable_value_test(self):
        # We haven't defined varK yet.
        varK = self.get_var('k')
        self.assertEqual(varK.unsigned, 0, "varK initialized too early?!")

        # Go to break point 2.1. k should be valid.
        self.runCmd('continue')
        varK = self.get_var('k')
        self.assertGreater(varK.unsigned, 0, "varK not initialized?!")

        # Go to breakpoint 2.2. k should still be valid. And we should be on the
        # other side of the force split.
        self.runCmd('continue')
        varK = self.get_var('k')
        self.assertGreater(varK.unsigned, 0, "varK not initialized?!")

        # Go to breakpoint 3. k should still be valid. We should be at the move
        # on the other side of the forceSplit.
        self.runCmd('continue')
        varK = self.get_var('k')
        self.assertGreater(varK.unsigned, 0, "varK not initialized?!")

        # We are now at break point 4. We have moved k, it should be empty.
        self.runCmd('continue')
        varK = self.get_var('k')
        self.assertIsNone(varK.value, "K is live but was moved?!")

        # Finally, we are on the other side of the final force split. Make sure
        # the value still isn't available.
        self.runCmd('continue')
        varK = self.get_var('k')
        self.assertIsNone(varK.value, "K is live but was moved?!")

        # Run so we hit the next breakpoint to jump to the next test's
        # breakpoint.
        self.runCmd('continue')
