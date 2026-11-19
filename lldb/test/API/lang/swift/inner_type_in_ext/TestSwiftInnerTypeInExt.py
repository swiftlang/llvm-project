import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftInnerTypeInExt(TestBase):
    @swiftTest
    # rdar://177460379
    @skipEmbeddedSwift
    def test(self):
        self.build()
        self.runCmd("settings set symbols.swift-enable-ast-context false")
        target, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"),
            extra_images=["ModBase"])
        frame = thread.GetSelectedFrame()
        var = frame.FindVariable("variable")
        self.assertTrue(var.IsValid(), "variable not found in frame")
        lldbutil.check_variable(self, var, use_dynamic=True, num_children=1)
        tag = var.GetChildMemberWithName("tag")
        self.assertTrue(tag.IsValid(), "tag child not found")
        lldbutil.check_variable(self, tag, value="42")
