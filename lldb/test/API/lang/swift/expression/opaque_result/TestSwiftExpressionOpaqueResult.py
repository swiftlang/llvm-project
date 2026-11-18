"""
Test expressions whose result is an opaque type the expression can look
through. The result variable must be lowered in the same type expansion
context as the rest of the expression.
"""

import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftExpressionOpaqueResult(TestBase):
    NO_DEBUG_INFO_TESTCASE = True

    @swiftTest
    @requireNotEmbeddedSwift
    def test(self):
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetSelectedFrame()

        # Inspect the static value: the dynamic type of an opaque result is
        # not resolved to its underlying type yet.
        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eNoDynamicValues)
        value = frame.EvaluateExpression("makeInlinable()", options)
        self.assertSuccess(value.GetError())
        self.assertEqual(value.GetChildMemberWithName("x").GetValue(), "1")
        self.assertEqual(value.GetChildMemberWithName("y").GetSummary(),
                         '"hello"')
