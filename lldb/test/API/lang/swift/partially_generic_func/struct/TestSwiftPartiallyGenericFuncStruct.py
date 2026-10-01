# TestSwiftPartiallyGenericFuncStruct.py
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


class TestSwiftPartiallyGenericFuncStruct(TestBase):

    def expr(self, frame, expression):
        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        value = frame.EvaluateExpression(expression, options)
        self.assertSuccess(value.GetError())
        return value

    @requireNotEmbeddedSwift
    @swiftTest
    def test(self):
        """Test that a generic struct argument of a generic function shows its fields"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        x = frame.FindVariable("x", lldb.eDynamicCanRunTarget)
        lldbutil.check_variable(self, x.GetChildMemberWithName("a"),
                                summary='"Hello world"')
        lldbutil.check_variable(self, x.GetChildMemberWithName("b"), value="12")
        x = self.expr(frame, "x")
        lldbutil.check_variable(self, x.GetChildMemberWithName("a"),
                                summary='"Hello world"')
        lldbutil.check_variable(self, x.GetChildMemberWithName("b"), value="12")
