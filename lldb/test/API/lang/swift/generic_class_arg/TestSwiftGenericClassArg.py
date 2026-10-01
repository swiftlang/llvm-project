import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftGenericClassArg(TestBase):

    def expr(self, frame, expression):
        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        value = frame.EvaluateExpression(expression, options)
        self.assertSuccess(value.GetError())
        return value

    @skipEmbeddedSwiftOnWindows
    @swiftTest
    def test(self):
        """Test that generic class and struct arguments resolve to their bound types"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        t1 = frame.FindVariable("t1").GetDynamicValue(lldb.eDynamicCanRunTarget)
        lldbutil.check_variable(self, t1.GetChildMemberWithName("x"), value="11223344")
        t2 = frame.FindVariable("t2").GetDynamicValue(lldb.eDynamicCanRunTarget)
        lldbutil.check_variable(self, t2.GetChildMemberWithName("x"), value="44332211")
        t1 = self.expr(frame, "t1")
        lldbutil.check_variable(self, t1.GetChildMemberWithName("x"), value="11223344")
        t2 = self.expr(frame, "t2")
        lldbutil.check_variable(self, t2.GetChildMemberWithName("x"), value="44332211")
