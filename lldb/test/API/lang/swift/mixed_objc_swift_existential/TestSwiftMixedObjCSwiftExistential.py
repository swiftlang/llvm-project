import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class SwiftMixedObjCSwiftExistentialTest(TestBase):
    @swiftTest
    @skipUnlessObjCInterop
    @skipEmbeddedSwift
    def test(self):
        """Reflection lowers class-bound @objc/Swift mixed existentials."""
        self.build()
        self.runCmd("settings set symbols.swift-enable-ast-context false")
        target, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        frame = thread.GetSelectedFrame()

        for name, expected in [("classConstrained", "42"), ("unconstrained", "43")]:
            var = frame.FindVariable(name)
            self.assertTrue(var.IsValid(), "%s not found in frame" % name)
            self.assertTrue(var.GetError().Success(), var.GetError().GetCString())
            payload = var.GetDynamicValue(
                lldb.eDynamicCanRunTarget
            ).GetChildMemberWithName("payload")
            lldbutil.check_variable(self, payload, value=expected)

        # The existential metatype of the same composition must also lower.
        metatype = frame.FindVariable("metatype")
        self.assertTrue(metatype.IsValid(), "metatype not found in frame")
        self.assertTrue(
            metatype.GetError().Success(), metatype.GetError().GetCString()
        )
