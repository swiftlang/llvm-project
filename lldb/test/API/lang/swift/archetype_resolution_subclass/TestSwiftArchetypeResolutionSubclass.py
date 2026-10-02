import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftArchetypeResolutionSubclass(TestBase):

    @requireNotEmbeddedSwift
    @swiftTest
    def test(self):
        """Test that a generic argument resolves to its dynamic subclass type"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        lldbutil.check_variable(self, frame.FindVariable("arg"), use_dynamic=True,
                                typename="a.Tinky")
