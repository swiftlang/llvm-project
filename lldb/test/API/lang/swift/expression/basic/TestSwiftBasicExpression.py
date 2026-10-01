import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftBasicExpression(TestBase):

    @skipEmbeddedSwift
    @swiftTest
    def test(self):
        """Test that an expression can define and instantiate a class"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        y = frame.EvaluateExpression(
            "class Y { var x : Int; init(_ x : Int) { self.x = x }}; Y(12)", options)
        self.assertSuccess(y.GetError())
        lldbutil.check_variable(self, y.GetChildMemberWithName("x"), value="12")
