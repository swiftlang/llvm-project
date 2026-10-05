# TestEmptySelf.py
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


class TestEmptySelf(TestBase):

    def expr(self, frame, expression):
        value = frame.EvaluateExpression(expression, lldb.SBExpressionOptions())
        self.assertSuccess(value.GetError())
        return value

    @swiftTest
    @skipEmbeddedSwiftOnWindows
    def test(self):
        """Test evaluating self in struct and class methods with no stored properties"""
        self.build()
        _, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break 1", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        lldbutil.check_variable(self, self.expr(frame, "self"),
                                typename="a.StructTest")

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 2", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        frame = threads[0].GetFrameAtIndex(0)
        lldbutil.check_variable(self, self.expr(frame, "self"),
                                typename="a.ClassTest")
