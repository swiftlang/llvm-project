import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftUninitializedVariable(TestBase):

    def expr(self, frame, expression):
        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        value = frame.EvaluateExpression(expression, options)
        self.assertSuccess(value.GetError())
        return value

    @requireNotEmbeddedSwift
    @swiftTest
    def test(self):
        """Test that uninitialized locals don't break frame variable or expressions"""
        self.build()
        target, process, thread, bkpt = lldbutil.run_to_source_breakpoint(
            self, "break 1", lldb.SBFileSpec("main.swift"))
        self.runCmd("frame variable adict",
                    msg="Frame variable of an uninitialized dict returns")

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 2", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        frame = threads[0].GetFrameAtIndex(0)
        # k2 and k3 are still uninitialized here; the expression must ignore them.
        lldbutil.check_variable(self, self.expr(frame, "c"), use_dynamic=True,
                                value="3")
