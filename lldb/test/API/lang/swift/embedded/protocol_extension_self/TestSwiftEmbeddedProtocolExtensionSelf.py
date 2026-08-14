import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftEmbeddedProtocolExtensionSelf(TestBase):
    @skipEmbeddedSwiftOnWindows
    @swiftTest
    def test(self):
        self.build()
        target, process, thread, bkpt = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))

        # The archetype is concretized while resolving the dynamic type, which
        # is what binding it against the metadata symbol makes possible.
        self.expect("frame variable self", substrs=["(a.C)", "number = 42"])
        lldbutil.check_variable(self, thread.frames[0].FindVariable("self"),
                                use_dynamic=True, typename="a.C",
                                num_children=1)

        lldbutil.continue_to_breakpoint(process, bkpt)
        self.expect("frame variable self", substrs=["(a.D)", "other = 99"])
        lldbutil.check_variable(self, thread.frames[0].FindVariable("self"),
                                use_dynamic=True, typename="a.D",
                                num_children=1)
