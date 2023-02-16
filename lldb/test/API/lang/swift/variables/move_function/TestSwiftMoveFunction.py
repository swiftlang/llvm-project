# TestSwiftMoveFunction.py
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

class TestSwiftMoveFunctionType(TestBase):

    # Skip on aarch64 linux: rdar://91005071
    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function(self):
        """Check that we properly show variables at various points of the CFG while
        stepping with the move function.
        """
        self.build()

        self.target, self.process, self.thread, self.bkpt = \
            lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    def setUp(self):
        TestBase.setUp(self)
        self.main_source = "main.swift"
        self.main_source_spec = lldb.SBFileSpec(self.main_source)
        self.exec_name = "a.out"
        self.runCmd("log enable lldb types -v")
        self.runCmd('setting set target.experimental.swift-read-metadata-from-file-cache false') 

    def get_var(self, name):
        frame = self.thread.frames[0]
        return frame.FindVariable(name)

    def do_check_copyable_value_test(self):
        # We haven't defined varK yet.
        varK = self.get_var('k')

        self.assertIsNone(varK.value, "varK initialized too early?!")

        # Go to break point 2. k should be valid.
        self.process.Continue()
        self.assertGreater(varK.unsigned, 0, "varK not initialized?!")

        # Go to breakpoint 3. k should no longer be valid.
        self.process.Continue()
        self.assertIsNone(varK.value, "K is live but was moved?!")

        # Run so we hit the next breakpoint to jump to the next test's
        # breakpoint.
        self.process.Continue()

    def do_check_copyable_var_test(self):
        # We haven't defined varK yet.
        varK = self.get_var('k')
        self.assertIsNone(varK.value, "varK initialized too early?!")

        # Go to break point 2. k should be valid.
        self.process.Continue()
        self.assertGreater(varK.unsigned, 0, "varK not initialized?!")

        # Go to breakpoint 3. We invalidated k
        self.process.Continue()
        self.assertIsNone(varK.value, "K is live but was moved?!")

        # Go to the last breakpoint and make sure that k is reinitialized
        # properly.
        self.process.Continue()
        self.assertGreater(varK.unsigned, 0, "varK not initialized")

        # Run so we hit the next breakpoint to go to the next test.
        self.process.Continue()

    def do_check_addressonly_value_test(self):
        # We haven't defined varK yet.
        varK = self.get_var('k')

        # Go to break point 2. k should be valid and m should not be. Since M is
        # a dbg.declare it is hard to test robustly that it is not initialized
        # so we don't do so. We have an additional llvm.dbg.addr test where we
        # move the other variable and show the correct behavior with
        # llvm.dbg.declare.
        self.process.Continue()
        self.assertGreater(varK.unsigned, 0, "var not initialized?!")

        # Go to breakpoint 3.
        self.process.Continue()
        self.assertEqual(varK.unsigned, 0,
                        "dbg thinks varK is live despite move?!")

        # Run so we hit the next breakpoint as part of the next test.
        self.process.Continue()

    def do_check_addressonly_var_test(self):
        varK = self.get_var('k')

        # Go to break point 2. k should be valid.
        self.process.Continue()
        self.assertGreater(varK.unsigned, 0, "varK not initialized?!")

        # Go to breakpoint 3. K was invalidated.
        self.process.Continue()
        self.assertIsNone(varK.value, "K is live but was moved?!")

        # Go to the last breakpoint and make sure that k is reinitialized
        # properly.
        self.process.Continue()
        self.assertGreater(varK.unsigned, 0, "varK not initialized")

        # Run so we hit the next breakpoint as part of the next test.
        self.process.Continue()

    def do_check_copyable_value_arg_test(self):
        # k is defined by the argument so it is valid.
        varK = self.get_var('k')
        self.assertGreater(varK.unsigned, 0, "varK not initialized?!")

        # Go to break point 2. k should be valid.
        self.process.Continue()
        self.assertGreater(varK.unsigned, 0, "varK not initialized?!")

        # Go to breakpoint 3. k should no longer be valid.
        self.process.Continue()
        #self.assertIsNone(varK.value, "K is live but was moved?!")

        # Run so we hit the next breakpoint to jump to the next test's
        # breakpoint.
        self.process.Continue()

    def do_check_copyable_var_arg_test(self):
        # k is already defined and is an argument.
        varK = self.get_var('k')
        self.assertGreater(varK.unsigned, 0, "varK not initialized?!")

        # Go to break point 2. k should be valid.
        self.process.Continue()
        self.assertGreater(varK.unsigned, 0, "varK not initialized?!")

        # Go to breakpoint 3. We invalidated k
        self.process.Continue()
        self.assertIsNone(varK.value, "K is live but was moved?!")

        # Go to the last breakpoint and make sure that k is reinitialized
        # properly.
        self.process.Continue()
        self.assertGreater(varK.unsigned, 0, "varK not initialized")

        # Run so we hit the next breakpoint to go to the next test.
        self.process.Continue()

    def do_check_addressonly_value_arg_test(self):
        # k is defined since it is an argument.
        varK = self.get_var('k')
        self.assertGreater(varK.unsigned, 0, "varK not initialized?!")

        # Go to break point 2. k should be valid and m should not be. Since M is
        # a dbg.declare it is hard to test robustly that it is not initialized
        # so we don't do so. We have an additional llvm.dbg.addr test where we
        # move the other variable and show the correct behavior with
        # llvm.dbg.declare.
        self.process.Continue()
        self.assertGreater(varK.unsigned, 0, "var not initialized?!")

        # Go to breakpoint 3.
        self.process.Continue()
        self.assertEqual(varK.unsigned, 0,
                        "dbg thinks varK is live despite move?!")

        # Run so we hit the next breakpoint as part of the next test.
        self.process.Continue()

    def do_check_addressonly_var_arg_test(self):
        varK = self.get_var('k')
        self.assertGreater(varK.unsigned, 0, "varK not initialized?!")

        # Go to break point 2. k should be valid.
        self.process.Continue()
        self.assertGreater(varK.unsigned, 0, "varK not initialized?!")

        # Go to breakpoint 3. K was invalidated.
        self.process.Continue()
        self.assertIsNone(varK.value, "K is live but was moved?!")

        # Go to the last breakpoint and make sure that k is reinitialized
        # properly.
        self.process.Continue()
        # There is some sort of bug here. We should have the value here. For now
        # leave the next line commented out and validate we are not seeing the
        # value so we can detect change in behavior.
        self.assertGreater(varK.unsigned, 0, "varK not initialized")

        # Run so we hit the next breakpoint as part of the next test.
        self.process.Continue()

    def do_check_copyable_value_ccf_true(self):
        varK = self.get_var('k')

        # Check at our start point that we do not have any state for varK and
        # then continue to our next breakpoint.
        self.assertIsNone(varK.value, "varK should not have a value?!")
        self.process.Continue()

        # At this breakpoint, k should be defined since we are going to do
        # something with it.
        self.assertIsNotNone(varK.value, "varK should have a value?!")
        self.process.Continue()

        # At this breakpoint, we are now in the conditional control flow part of
        # the loop. Make sure that we can see k still.
        self.assertIsNotNone(varK.value, "varK should have a value?!")
        self.process.Continue()

        # Ok, we just performed the move. k should not be no longer initialized.
        self.assertIsNone(varK.value, "varK should not have a value?!")
        self.process.Continue()

        # Finally we left the conditional control flow part of the function. k
        # should still be None.
        self.assertIsNone(varK.value, "varK should not have a value!")

        # Run again so we go and run to the next test.
        self.process.Continue()

    def do_check_copyable_value_ccf_false(self):
        varK = self.get_var('k')

        # Check at our start point that we do not have any state for varK and
        # then continue to our next breakpoint.
        self.assertIsNone(varK.value, "varK should not have a value?!")
        self.process.Continue()

        # At this breakpoint, k should be defined since we are going to do
        # something with it.
        self.assertIsNotNone(varK.value, "varK should have a value?!")
        self.process.Continue()

        # At this breakpoint, we are now past the end of the conditional
        # statement. We know due to the move checking that k can not have any
        # uses that are reachable from the move. So it is safe to always not
        # provide the value here.
        self.assertIsNone(varK.value, "varK should have a value?!")

        # Run again so we go and run to the next test.
        self.process.Continue()

    def do_check_copyable_var_ccf_true_reinit_out_block(self):
        varK = self.get_var('k')

        # At first we should not have a value for k.
        self.assertEqual(varK.unsigned, 0, "varK should be nullptr!")
        self.process.Continue()

        # Now we are in the conditional true block. K should be defined since we
        # are on the move itself.
        self.assertGreater(varK.unsigned, 0, "varK should not be nullptr!")
        self.process.Continue()

        # Now we have executed the move and we are about to run code using
        # m. Make sure that K is not available!
        self.assertEqual(varK.unsigned, 0,
                         "varK was already moved! Should be nullptr")
        self.process.Continue()

        # We are now out of the conditional lexical block on the line of code
        # that redefines k. k should still be not available.
        self.assertEqual(varK.unsigned, 0,
                         "varK was already moved! Should be nullptr")
        self.process.Continue()

        # Ok, we have now reinit k and are about to call a method on it. We
        # should be valid now.
        self.assertGreater(varK.unsigned, 0,
                           "varK should have be reinitialized?!")

        # Run again so we go and run to the next test.
        self.process.Continue()

    def do_check_copyable_var_ccf_true_reinit_in_block(self):
        varK = self.get_var('k')

        # At first we should not have a value for k.
        self.assertEqual(varK.unsigned, 0, "varK should be nullptr!")
        self.process.Continue()

        # Now we are in the conditional true block. K should be defined since we
        # are on the move itself.
        self.assertGreater(varK.unsigned, 0, "varK should not be nullptr!")
        self.process.Continue()

        # Now we have executed the move and we are about to reinit k but have
        # not yet. Make sure we are not available!
        self.assertEqual(varK.unsigned, 0,
                         "varK was already moved! Should be nullptr")
        self.process.Continue()

        # We are now still inside the conditional part of the code, but have
        # reinitialized varK.
        self.assertGreater(varK.unsigned, 0,
                           "varK was reinit! Should be valid value!")
        self.process.Continue()

        # We now have left the conditional part of the function. k should still
        # be available.
        self.assertGreater(varK.unsigned, 0,
                           "varK should have be reinitialized?!")

        # Run again so we go and run to the next test.
        self.process.Continue()

    def do_check_copyable_var_ccf_false_reinit_out_block(self):
        varK = self.get_var('k')

        # At first we should not have a value for k.
        self.assertEqual(varK.unsigned, 0, "varK should be nullptr!")
        self.process.Continue()

        # Now we are right above the beginning of the false check. varK should
        # still be valid.
        self.assertGreater(varK.unsigned, 0, "varK should not be nullptr!")
        self.process.Continue()

        # Now we are after the conditional part of the code on the reinit
        # line. Since this is reachable from the move and we haven't reinit yet,
        # k should not be available.
        self.assertEqual(varK.unsigned, 0,
                         "varK was already moved! Should be nullptr")
        self.process.Continue()

        # Ok, we have now reinit k and are about to call a method on it. We
        # should be valid now.
        self.assertGreater(varK.unsigned, 0,
                           "varK should have be reinitialized?!")

        # Run again so we go and run to the next test.
        self.process.Continue()

    def do_check_copyable_var_ccf_false_reinit_in_block(self):
        varK = self.get_var('k')

        # At first we should not have a value for k.
        self.assertEqual(varK.unsigned, 0, "varK should be nullptr!")
        self.process.Continue()

        # Now we are on the doSomething above the false check. So varK should be
        # valid.
        self.assertGreater(varK.unsigned, 0, "varK should not be nullptr!")
        self.process.Continue()

        # Now we are after the conditional scope. Since k was reinitialized in
        # the conditional scope, along all paths we are valid so varK should
        # still be available.
        self.assertGreater(varK.unsigned, 0,
                           "varK should not be nullptr?!")

        # Run again so we go and run to the next test.
        self.process.Continue()


    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_0(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_1(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_2(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_3(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_4(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_5(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_6(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_7(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_8(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_9(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_10(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_11(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_12(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_13(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_14(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_15(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_16(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_17(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_18(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_19(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_20(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_21(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_22(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_23(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_24(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_25(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_26(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_27(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_28(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_29(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_30(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_31(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_32(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_33(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_34(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_35(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_36(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_37(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_38(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_39(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_40(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_41(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_42(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_43(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_44(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_45(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_46(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_47(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_48(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_49(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_50(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_51(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_52(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_53(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_54(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_55(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_56(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_57(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_58(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_59(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_60(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_61(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_62(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_63(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_64(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_65(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_66(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_67(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_68(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_69(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_70(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_71(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_72(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_73(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_74(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_75(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_76(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_77(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_78(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_79(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_80(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_81(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_82(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_83(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_84(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_85(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_86(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_87(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_88(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_89(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_90(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_91(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_92(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_93(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_94(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_95(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_96(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_97(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_98(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

    @skipIf(archs=['aarch64'], oslist=['linux'])
    @swiftTest
    def test_swift_move_function_99(self):
        self.build()

        self.target, self.process, self.thread, self.bkpt =             lldbutil.run_to_source_breakpoint(
                self, 'Set breakpoint', lldb.SBFileSpec('main.swift'))

        self.do_check_copyable_value_test()
        self.do_check_copyable_var_test()
        self.do_check_addressonly_value_test()
        self.do_check_addressonly_var_test()

        # argument simple tests
        self.do_check_copyable_value_arg_test()
        self.do_check_copyable_var_arg_test()
        self.do_check_addressonly_value_arg_test()
        self.do_check_addressonly_var_arg_test()

        # ccf is conditional control flow
        self.do_check_copyable_value_ccf_true()
        self.do_check_copyable_value_ccf_false()
        self.do_check_copyable_var_ccf_true_reinit_out_block()
        self.do_check_copyable_var_ccf_true_reinit_in_block()
        self.do_check_copyable_var_ccf_false_reinit_out_block()
        self.do_check_copyable_var_ccf_false_reinit_in_block()

