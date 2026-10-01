import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftArrayTupleResilient(TestBase):

    def expr(self, frame, expression):
        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        value = frame.EvaluateExpression(expression, options)
        self.assertSuccess(value.GetError())
        return value

    @requireNotEmbeddedSwift
    @swiftTest
    @skipIfLinux
    @expectedFailureAll(archs=['arm64_32'], bugnumber="<rdar://problem/58065423>")
    def test(self):
        """Test that arrays of tuples with resilient element types are printed"""
        self.build()
        _, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break 1", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        for patatino in [frame.GetValueForVariablePath("patatino"),
                         self.expr(frame, "patatino")]:
            element = patatino.GetChildAtIndex(0)
            lldbutil.check_variable(self, element.GetChildAtIndex(0), summary="3 bytes")
            lldbutil.check_variable(self, element.GetChildAtIndex(1), value="1001")

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 2", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        frame = threads[0].GetFrameAtIndex(0)
        for tinky in [frame.GetValueForVariablePath("tinky"),
                      self.expr(frame, "tinky")]:
            element = tinky.GetChildAtIndex(0)
            lldbutil.check_variable(self, element.GetChildAtIndex(0), summary="3 bytes")
            lldbutil.check_variable(self, element.GetChildAtIndex(1), summary="1 byte")
