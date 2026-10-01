# TestSwiftGenericTuple.py
#
# This source file is part of the Swift.org open source project
#
# Copyright (c) 2018 Apple Inc. and the Swift project authors
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


class TestSwiftGenericTuple(TestBase):

    def expr(self, frame, expression):
        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        value = frame.EvaluateExpression(expression, options)
        self.assertSuccess(value.GetError())
        return value

    @skipEmbeddedSwiftOnWindows
    @swiftTest
    def test(self):
        """Test that generic values and tuples resolve to their bound types"""
        self.build()
        # Embedded Swift specializes generic code, so these values are concrete
        # there and have no dynamic type to resolve.
        use_dynamic = not self.isEmbeddedSwift()
        target, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break 1", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        lldbutil.check_variable(self, self.expr(frame, "t"), summary='"hello"')
        lldbutil.check_variable(self, self.expr(frame, "x"), summary='"hello"')
        lldbutil.check_variable(self, frame.FindVariable("t"), use_dynamic=use_dynamic,
                                typename="Swift.String", summary='"hello"')
        lldbutil.check_variable(self, frame.FindVariable("x"), use_dynamic=use_dynamic,
                                summary='"hello"')

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 2", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        frame = threads[0].GetFrameAtIndex(0)
        t = self.expr(frame, "t")
        lldbutil.check_variable(self, t.GetChildAtIndex(0), summary='"hello"')
        lldbutil.check_variable(self, t.GetChildAtIndex(1), summary='"hello"')
        lldbutil.check_variable(self, self.expr(frame, "y"), summary='"hello"')
        t = frame.FindVariable("t")
        lldbutil.check_variable(self, t, use_dynamic=use_dynamic,
                                typename="(Swift.String, Swift.String)")
        t = t.GetDynamicValue(lldb.eDynamicCanRunTarget)
        lldbutil.check_variable(self, t.GetChildAtIndex(0), summary='"hello"')
        lldbutil.check_variable(self, t.GetChildAtIndex(1), summary='"hello"')
        lldbutil.check_variable(self, frame.FindVariable("y"), use_dynamic=use_dynamic,
                                summary='"hello"')

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 3", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        frame = threads[0].GetFrameAtIndex(0)
        t = self.expr(frame, "t")
        lldbutil.check_variable(self, t, typename="(Swift.Int32, Swift.Int64)")
        lldbutil.check_variable(self, t.GetChildAtIndex(0), value="111")
        lldbutil.check_variable(self, t.GetChildAtIndex(1), value="222")
        lldbutil.check_variable(self, self.expr(frame, "y"), value="222")
        t = frame.FindVariable("t")
        lldbutil.check_variable(self, t, use_dynamic=use_dynamic,
                                typename="(Swift.Int32, Swift.Int64)")
        t = t.GetDynamicValue(lldb.eDynamicCanRunTarget)
        lldbutil.check_variable(self, t.GetChildAtIndex(0), value="111")
        lldbutil.check_variable(self, t.GetChildAtIndex(1), value="222")
        lldbutil.check_variable(self, frame.FindVariable("y"), use_dynamic=use_dynamic,
                                value="222")
