import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil


class TestMarkerProtocolExistential(lldbtest.TestBase):
    # main.swift calls one function per scenario, so one process visits them
    # all in order.
    @requireNotEmbeddedSwift
    @swiftTest
    def test(self):
        self.build()
        target, self.inferior, _, bkpt = lldbutil.run_to_source_breakpoint(
            self, "break marker only", lldb.SBFileSpec("main.swift")
        )
        target.BreakpointDelete(bkpt.GetID())
        s = self.frame().FindVariable("v")
        self.assertEqual(s.GetTypeName(), "a.S")
        x = s.GetChildMemberWithName("x")
        lldbutil.check_variable(self, x, value="42")

        self.continue_to("break composition")
        t = self.frame().FindVariable("v")
        a = t.GetChildMemberWithName("a")
        lldbutil.check_variable(self, a, value="10")

        self.continue_to("break two markers")
        u = self.frame().FindVariable("v")
        self.assertEqual(u.GetTypeName(), "a.U")
        b = u.GetChildMemberWithName("b")
        lldbutil.check_variable(self, b, value="20")

        self.continue_to("break any and marker")
        s = self.frame().FindVariable("v")
        self.assertEqual(s.GetTypeName(), "a.S")
        x = s.GetChildMemberWithName("x")
        lldbutil.check_variable(self, x, value="42")

        self.continue_to("break any marker and non marker")
        v = self.frame().FindVariable("v")
        d = v.GetChildMemberWithName("d")
        lldbutil.check_variable(self, d, value="30")

    def continue_to(self, bkpt_pattern):
        threads = lldbutil.continue_to_source_breakpoint(
            self, self.inferior, bkpt_pattern, lldb.SBFileSpec("main.swift")
        )
        self.assertEqual(len(threads), 1, f"no stop at {bkpt_pattern}")
