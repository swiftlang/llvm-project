import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftProtocolCompositionSubst(TestBase):
    @skipEmbeddedSwift
    @swiftTest
    def test(self):
        self.build()
        self.runCmd("settings set symbols.swift-enable-ast-context false")
        target, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        frame = thread.GetSelectedFrame()

        for name in ("variable", "paramOnly", "classBoundParam"):
            var = frame.FindVariable(name)
            self.assertTrue(var.IsValid(), f"{name} not found in frame")
            typename = var.GetType().GetDisplayTypeName()
            self.assertIn("Int", typename, f"{name} type {typename!r} missing Int")
            self.assertNotIn("$τ", typename, f"{name} type {typename!r} has unsubstituted archetype")
            self.assertNotIn(".U>", typename, f"{name} type {typename!r} has unsubstituted U")

            dyn = var.GetDynamicValue(lldb.eDynamicCanRunTarget)
            payload = dyn.GetChildMemberWithName("payload")
            lldbutil.check_variable(self, payload, value="42")
