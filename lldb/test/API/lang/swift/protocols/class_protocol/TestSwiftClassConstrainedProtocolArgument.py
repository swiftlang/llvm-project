"""
Test that variables passed in as a class constrained protocol type
are correctly printed.
"""
import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftClassConstrainedProtocolArgument(TestBase):

    @requireNotEmbeddedSwift
    @swiftTest
    @requireSwiftObjCInterop
    def test(self):
        """Test that a generic argument bound to an NSObject subclass resolves"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        # Evaluate twice; a repeated evaluation must give the same result.
        for _ in range(2):
            value = frame.EvaluateExpression("input", options)
            self.assertSuccess(value.GetError())
            lldbutil.check_variable(self, value, typename="a.ShouldBeWhy")
            # The NSObject base class child can't be looked up by name.
            nsobject = value.GetChildAtIndex(0)
            self.assertEqual(nsobject.GetName(), "ObjectiveC.NSObject")
            lldbutil.check_variable(self, nsobject.GetChildMemberWithName("isa"),
                                    summary="a.ShouldBeWhy")
            lldbutil.check_variable(
                self, value.GetChildMemberWithName("before_why"),
                value="4277009102")
            lldbutil.check_variable(
                self, value.GetChildMemberWithName("why"), value="10")
