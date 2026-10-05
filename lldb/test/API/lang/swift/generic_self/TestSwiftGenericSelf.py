# TestSwiftGenericSelf.py
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
import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftGenericSelf(TestBase):

    def expr(self, frame, expression):
        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        value = frame.EvaluateExpression(expression, options)
        self.assertSuccess(value.GetError())
        return value

    @skipEmbeddedSwiftOnWindows
    @swiftTest
    def test(self):
        """Test that self resolves to its bound generic type in generic methods"""
        self.build()
        # Embedded Swift specializes generic code, so these values are concrete
        # there and have no dynamic type to resolve.
        use_dynamic = not self.isEmbeddedSwift()
        target, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break 1", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        lldbutil.check_variable(self, self.expr(frame, "my_t"), use_dynamic=use_dynamic,
                                value="3735928559")
        lldbutil.check_variable(self, frame.GetValueForVariablePath("self.my_t"),
                                value="3735928559")
        my_t = self.expr(frame, "self").GetChildMemberWithName("my_t")
        lldbutil.check_variable(self, my_t, value="3735928559")
        my_t = frame.FindVariable("self").GetChildMemberWithName("my_t")
        lldbutil.check_variable(self, my_t, value="3735928559")

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 2", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        frame = threads[0].GetFrameAtIndex(0)
        s = self.expr(frame, "self")
        lldbutil.check_variable(self, s, use_dynamic=use_dynamic, typename="a.S<Swift.Int>")
        lldbutil.check_variable(self, s.GetChildMemberWithName("a"), value="12")
        s = frame.FindVariable("self")
        lldbutil.check_variable(self, s, use_dynamic=use_dynamic, typename="a.S<Swift.Int>")
        lldbutil.check_variable(self, s.GetChildMemberWithName("a"), value="12")
