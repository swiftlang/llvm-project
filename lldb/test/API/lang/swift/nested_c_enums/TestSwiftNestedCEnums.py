import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftNestedCEnums(TestBase):

    def expr(self, frame, expression):
        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        value = frame.EvaluateExpression(expression, options)
        self.assertSuccess(value.GetError())
        return value

    @requireNotEmbeddedSwift
    @swiftTest
    def test(self):
        """Test that enum cases with enum payloads show the payload case"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        for name, case, payload in [("x", "Case1", "A"), ("y", "Case1", "B"),
                                    ("w", "Case2", "C"), ("z", "Case2", "D")]:
            for v in [frame.GetValueForVariablePath(name), self.expr(frame, name)]:
                lldbutil.check_variable(self, v, typename="a.SuperEnum")
                lldbutil.check_variable(self, v.GetChildMemberWithName(case),
                                        value=payload)
