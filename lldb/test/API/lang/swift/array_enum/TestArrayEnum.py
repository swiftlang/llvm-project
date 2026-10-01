import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestArrayEnum(TestBase):

    def expr(self, frame, expression):
        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        value = frame.EvaluateExpression(expression, options)
        self.assertSuccess(value.GetError())
        return value

    @requireNotEmbeddedSwift
    @swiftTest
    def test(self):
        """Test that an optional enum field shows its case in a struct and in an array"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        for a in [frame.FindVariable("a"), self.expr(frame, "a")]:
            lldbutil.check_variable(self, a, typename="a.y")
            lldbutil.check_variable(self, a.GetChildMemberWithName("z"),
                                    value="patatino")
        for j in [frame.FindVariable("j"), self.expr(frame, "j")]:
            z = j.GetChildAtIndex(0).GetChildMemberWithName("z")
            lldbutil.check_variable(self, z, value="patatino")
