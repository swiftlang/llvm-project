import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftGenericErrorTuple(TestBase):

    def check_payload_errors(self, t):
        lldbutil.check_variable(
            self, t.GetChildAtIndex(0).GetChildMemberWithName("x"), value="23")
        # The second element is a MyOtherErr, whose x lives in its base class.
        other = t.GetChildAtIndex(1).GetDynamicValue(lldb.eDynamicCanRunTarget)
        base = other.GetChildAtIndex(0)
        self.assertEqual(base.GetName(), "a.PayloadErr")
        lldbutil.check_variable(self, base.GetChildMemberWithName("x"), value="42")

    @swiftTest
    @skipEmbeddedSwiftOnWindows
    def test(self):
        """Test that generic tuples of errors resolve to their bound types"""
        self.build()
        # Embedded Swift specializes generic code, so these values are concrete
        # there and have no dynamic type to resolve.
        use_dynamic = not self.isEmbeddedSwift()
        target, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break 1", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        t = frame.FindVariable("tuple")
        lldbutil.check_variable(self, t, use_dynamic=use_dynamic,
                                typename="(Swift.Error, Swift.Int)")
        t = t.GetDynamicValue(lldb.eDynamicCanRunTarget)
        lldbutil.check_variable(self, t.GetChildAtIndex(0), use_dynamic=True,
                                value="Topolino")
        lldbutil.check_variable(self, t.GetChildAtIndex(1), value="42")

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 2", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        frame = threads[0].GetFrameAtIndex(0)
        t = frame.FindVariable("tuple")
        lldbutil.check_variable(self, t, use_dynamic=use_dynamic,
                                typename="(a.PayloadErr, a.PayloadErr)")
        self.check_payload_errors(t.GetDynamicValue(lldb.eDynamicCanRunTarget))

        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        t = frame.EvaluateExpression("tuple", options)
        self.assertSuccess(t.GetError())
        lldbutil.check_variable(self, t, typename="(a.PayloadErr, a.PayloadErr)")
        self.check_payload_errors(t)
