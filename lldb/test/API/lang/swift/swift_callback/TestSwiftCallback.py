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


class TestSwiftCallback(TestBase):

    def expr(self, frame, expression):
        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        value = frame.EvaluateExpression(expression, options)
        self.assertSuccess(value.GetError())
        return value

    @swiftTest
    @skipEmbeddedSwiftOnWindows
    def test(self):
        """Test inout and closure arguments in a function taking a callback"""
        self.build()
        target, process, thread, bkpt = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        lldbutil.check_variable(self, frame.FindVariable("stop"),
                                typename="Swift.Bool", summary="false")
        lldbutil.check_variable(self, self.expr(frame, "stop"), typename="Swift.Bool",
                                summary="false")
        # The closure's function name is only in the value string, next to its address.
        self.expect(
            'frame variable -d run -- body',
            substrs=['closure #1 (Swift.String, Swift.Int, inout Swift.Bool) -> ()'])
