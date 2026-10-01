import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestOpaqueExistentials(TestBase):

    def expr(self, frame, expression):
        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        value = frame.EvaluateExpression(expression, options)
        self.assertSuccess(value.GetError())
        return value

    @requireNotEmbeddedSwift
    @swiftTest
    def test(self):
        """Test that a protocol existential resolves to its dynamic struct type"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        for tinky in [frame.GetValueForVariablePath("tinky"),
                      self.expr(frame, "tinky")]:
            lldbutil.check_variable(self, tinky, use_dynamic=True, typename="a.S")
            tinky = tinky.GetDynamicValue(lldb.eDynamicCanRunTarget)
            lldbutil.check_variable(self, tinky.GetChildMemberWithName("type"),
                                    summary='"pata"')
            lldbutil.check_variable(self, tinky.GetChildMemberWithName("stringValue"),
                                    summary='"tino"')
