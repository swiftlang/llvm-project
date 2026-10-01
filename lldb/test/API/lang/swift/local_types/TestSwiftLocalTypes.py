# TestSwiftLocalTypes.py
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


class TestSwiftLocalTypes(TestBase):

    def expr(self, frame, expression, dynamic=False):
        options = lldb.SBExpressionOptions()
        if dynamic:
            options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        value = frame.EvaluateExpression(expression, options)
        self.assertSuccess(value.GetError())
        return value

    def check_foo(self, f):
        lldbutil.check_variable(self, f.GetChildMemberWithName("a"), value="234")
        lldbutil.check_variable(self, f.GetChildMemberWithName("b"), value="1.25")

    def check_bar(self, b):
        lldbutil.check_variable(self, b.GetChildMemberWithName("c"), value="48")
        lldbutil.check_variable(self, b.GetChildMemberWithName("d"), summary='"Hello"')

    def check_derived(self, c):
        base = c.GetChildAtIndex(0)
        lldbutil.check_variable(self, base.GetChildMemberWithName("a"), value="1")
        lldbutil.check_variable(self, c.GetChildMemberWithName("b"), value="2")

    def continue_to(self, process, marker):
        threads = lldbutil.continue_to_source_breakpoint(
            self, process, marker, lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        return threads[0].GetFrameAtIndex(0)

    @skipEmbeddedSwiftOnWindows
    @swiftTest
    def test(self):
        """Test that values of function-local struct and class types can be inspected"""
        self.build()
        target, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break 1", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        self.check_foo(frame.FindVariable("f"))

        frame = self.continue_to(process, "break 2")
        self.check_bar(frame.FindVariable("b"))

        frame = self.continue_to(process, "break 3")
        self.check_derived(frame.FindVariable("c"))

        frame = self.continue_to(process, "break 4")
        self.check_foo(self.expr(frame, "f"))

        frame = self.continue_to(process, "break 5")
        self.check_bar(self.expr(frame, "b"))

        frame = self.continue_to(process, "break 6")
        self.check_derived(self.expr(frame, "c", dynamic=True))
