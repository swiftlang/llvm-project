# TestSwiftGenericStructDebugInfoGenericApply.py
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


class TestSwiftGenericStructDebugInfoGenericApply(TestBase):

    def expr(self, frame, expression):
        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        value = frame.EvaluateExpression(expression, options)
        self.assertSuccess(value.GetError())
        return value

    @swiftTest
    @skipEmbeddedSwiftOnWindows
    def test(self):
        """Test that a generic closure argument resolves to its bound type"""
        self.build()
        # Embedded Swift specializes generic code, so these values are concrete
        # there and have no dynamic type to resolve.
        use_dynamic = not self.isEmbeddedSwift()
        target, process, thread, bkpt = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        self.expect('expr -o -d run -- arg', substrs=['3735928559'])
        lldbutil.check_variable(self, self.expr(frame, "arg"), typename="Swift.Int",
                                value="3735928559")
        lldbutil.check_variable(self, frame.FindVariable("arg"), use_dynamic=use_dynamic,
                                typename="Swift.Int", value="3735928559")
