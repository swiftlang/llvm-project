# TestSwiftPartiallyGenericFuncContinuation.py
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


class TestSwiftPartiallyGenericFuncContinuation(TestBase):

    def expr(self, frame, expression):
        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        value = frame.EvaluateExpression(expression, options)
        self.assertSuccess(value.GetError())
        return value

    def check_continuation(self, x):
        lldbutil.check_variable(self, x.GetChildMemberWithName("magicToken"),
                                summary='"Hello World"')
        for name in ["f", "failable", "perfMetric"]:
            lldbutil.check_variable(self, x.GetChildMemberWithName(name),
                                    summary="nil")

    @requireNotEmbeddedSwift
    @swiftTest
    def test(self):
        """Test that a generic struct used in a generic closure shows its fields"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        self.check_continuation(frame.FindVariable("x", lldb.eDynamicCanRunTarget))
        self.check_continuation(self.expr(frame, "x"))
