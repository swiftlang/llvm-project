# TestSwiftGenericStructDebugInfoGenericArray.py
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


class TestSwiftGenericStructDebugInfoGenericArray(TestBase):

    def expr(self, frame, expression):
        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        value = frame.EvaluateExpression(expression, options)
        self.assertSuccess(value.GetError())
        return value

    @requireNotEmbeddedSwift
    @swiftTest
    def test(self):
        """Test that an array of generic structs shows the elements' fields"""
        self.build()
        target, process, thread, bkpt = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        self.expect('expr -o -d run -- arg', substrs=['x : 3735928559'])
        arg = self.expr(frame, "arg")
        lldbutil.check_variable(
            self, arg.GetChildAtIndex(0).GetChildMemberWithName("x"),
            value="3735928559")
        arg = frame.FindVariable("arg").GetDynamicValue(lldb.eDynamicCanRunTarget)
        lldbutil.check_variable(
            self, arg.GetChildAtIndex(0).GetChildMemberWithName("x"),
            value="3735928559")
