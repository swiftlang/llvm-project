import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftOpaqueNongeneric(TestBase):
    NO_DEBUG_INFO_TESTCASE = True

    @skipEmbeddedSwift
    @swiftTest
    def test(self):
        self.build()
        self.runCmd("settings set symbols.swift-enable-ast-context false")
        target, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"),
            extra_images=["OpaqueLib"])
        frame = thread.GetSelectedFrame()
        var = frame.FindVariable("variable")
        self.assertTrue(var.IsValid(), "variable not found in frame")
        self.assertGreater(var.GetNumChildren(), 0,
                           "opaque-return underlying type failed to lower")
        typename = var.GetTypeName()
        self.assertIn("Int", typename,
                      "expected substituted Int in resolved type, got " + typename)
        lldbutil.check_variable(self, var.GetChildMemberWithName("value"),
                                value="42")
