import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftGenericExistentials(TestBase):

    def expr(self, frame, expression):
        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        value = frame.EvaluateExpression(expression, options)
        self.assertSuccess(value.GetError())
        return value

    def check_x(self, value, typename, fields):
        lldbutil.check_variable(self, value, use_dynamic=True, typename=typename)
        value = value.GetDynamicValue(lldb.eDynamicCanRunTarget)
        for name, field_value in fields:
            lldbutil.check_variable(self, value.GetChildMemberWithName(name),
                                    value=field_value)

    def check_frame(self, frame, typename, fields):
        self.check_x(frame.FindVariable("x"), typename, fields)
        self.check_x(self.expr(frame, "x"), typename, fields)

    @swiftTest
    def test(self):
        """Test that generic arguments bound to existentials resolve to the stored type"""
        self.build()
        target, process, thread, bkpt = lldbutil.run_to_source_breakpoint(
            self, "break 1", lldb.SBFileSpec("main.swift"))
        # T bound to Any.
        self.check_frame(thread.GetFrameAtIndex(0), "a.MyClass", [("x", "23")])

        # T bound to AnyObject.
        threads = lldbutil.continue_to_breakpoint(process, bkpt)
        self.assertEqual(len(threads), 1)
        self.check_frame(threads[0].GetFrameAtIndex(0), "a.MyClass", [("x", "23")])

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 2", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        self.check_frame(threads[0].GetFrameAtIndex(0), "a.MyStruct", [("x", "23")])

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 3", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        self.check_frame(threads[0].GetFrameAtIndex(0), "a.MyBigStruct",
                         [("x", "23"), ("y", "24"), ("z", "25"), ("w", "26")])
