import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftOpaqueReturnTypes(TestBase):

    @requireNotEmbeddedSwift
    @swiftTest
    @skipIf(macos_version=["<", "10.15"])
    def test(self):
        """Test that values of opaque result types show their underlying types"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        for name in ["a", "b", "c", "f"]:
            lldbutil.check_variable(self, frame.FindVariable(name),
                                    typename="Swift.Int", value="0")
        for name in ["d", "g", "i"]:
            v = frame.FindVariable(name)
            lldbutil.check_variable(self, v, typename="a.Wrapper<Swift.Int>")
            lldbutil.check_variable(self, v.GetChildMemberWithName("value"), value="0")
        for name in ["e", "h", "j"]:
            v = frame.FindVariable(name)
            lldbutil.check_variable(self, v,
                                    typename="a.Wrapper<a.Wrapper<Swift.Int>>")
            v = v.GetChildMemberWithName("value").GetChildMemberWithName("value")
            lldbutil.check_variable(self, v, value="0")
