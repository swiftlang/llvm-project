# TestSwiftPORecursiveBehavior.py
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


class TestSwiftGenericStructDebugInfoGenericFlatMap(TestBase):

    def expr(self, frame, expression):
        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        value = frame.EvaluateExpression(expression, options)
        self.assertSuccess(value.GetError())
        return value

    def check_product(self, product):
        lldbutil.check_variable(self, product.GetChildMemberWithName("name"),
                                summary='"Coffee"')
        lldbutil.check_variable(self, product.GetChildMemberWithName("ID"),
                                summary='"1"')

    def check_value_and_indices(self, v):
        lldbutil.check_variable(self, v.GetChildMemberWithName("originalIndex"),
                                value="0")
        lldbutil.check_variable(self, v.GetChildMemberWithName("filteredIndex"),
                                value="0")
        self.check_product(v.GetChildMemberWithName("value"))

    @requireNotEmbeddedSwift
    @swiftTest
    def test(self):
        """Test generic struct arguments of closures passed to flatMap"""
        self.build()
        target, process, thread, bkpt = lldbutil.run_to_source_breakpoint(
            self, "break 1", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        self.expect('expr -o -d run -- tuple',
                    substrs=['originalIndex : 0', 'filteredIndex : 0',
                             'name : "Coffee"', 'ID : "1"'])
        self.check_value_and_indices(self.expr(frame, "tuple"))
        self.check_value_and_indices(
            frame.FindVariable("tuple").GetDynamicValue(lldb.eDynamicCanRunTarget))

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 2", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        frame = threads[0].GetFrameAtIndex(0)
        self.expect('expr -o -d run -- value', substrs=['name : "Coffee"', 'ID : "1"'])
        self.check_product(self.expr(frame, "value"))
        self.check_product(
            frame.FindVariable("value").GetDynamicValue(lldb.eDynamicCanRunTarget))
