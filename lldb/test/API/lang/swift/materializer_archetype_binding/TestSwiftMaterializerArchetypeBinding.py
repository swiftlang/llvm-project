# TestSwiftMaterializerArchetypeBinding.py
#
# This source file is part of the Swift.org open source project
#
# Copyright (c) 2014 - 2018 Apple Inc. and the Swift project authors
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


class TestSwiftMaterializerArchetypeBinding(TestBase):

    def expr(self, frame, expression):
        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        value = frame.EvaluateExpression(expression, options)
        self.assertSuccess(value.GetError())
        return value

    def check_params(self, frame):
        param = frame.FindVariable("param")
        lldbutil.check_variable(self, param, use_dynamic=True,
                                typename="a.GenericImpl")
        param = param.GetDynamicValue(lldb.eDynamicCanRunTarget)
        lldbutil.check_variable(self, param.GetChildMemberWithName("test"),
                                summary='"test test"')
        param = self.expr(frame, "param")
        lldbutil.check_variable(self, param, typename="a.GenericImpl")
        lldbutil.check_variable(self, param.GetChildMemberWithName("test"),
                                summary='"test test"')
        lldbutil.check_variable(self, frame.FindVariable("anotherParam"),
                                typename="Swift.String", summary='"just a string"')
        lldbutil.check_variable(self, self.expr(frame, "anotherParam"),
                                typename="Swift.String", summary='"just a string"')

    @requireNotEmbeddedSwift
    @swiftTest
    def test(self):
        """Test that generic parameters of protocol extension and class methods resolve"""
        self.build()
        target, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break 1", lldb.SBFileSpec("main.swift"))
        self.check_params(thread.GetFrameAtIndex(0))

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 2", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        self.check_params(threads[0].GetFrameAtIndex(0))
