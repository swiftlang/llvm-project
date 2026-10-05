import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestEnumTagged(TestBase):

    def check_payload(self, var, case, payload_type, field, value):
        variant = var.GetChildMemberWithName("variant")
        lldbutil.check_variable(self, variant, value=case)
        payload = variant.GetChildMemberWithName(case)
        lldbutil.check_variable(self, payload, use_dynamic=True, typename=payload_type)
        lldbutil.check_variable(self, payload.GetChildMemberWithName(field), value=value)

    @swiftTest
    @skipEmbeddedSwiftOnWindows
    def test(self):
        """Test that the payload of a private multi-payload enum with class payloads is resolved"""
        self.build()
        target, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)

        x = target.FindFirstGlobalVariable("x").GetDynamicValue(lldb.eDynamicCanRunTarget)
        self.check_payload(x, "x", "a.A.X", "xx", "42")
        self.check_payload(frame.EvaluateExpression("x", options), "x", "a.A.X", "xx", "42")

        y = target.FindFirstGlobalVariable("y").GetDynamicValue(lldb.eDynamicCanRunTarget)
        self.check_payload(y, "y", "a.A.Y", "yy", "39")
        self.check_payload(frame.EvaluateExpression("y", options), "y", "a.A.Y", "yy", "39")
