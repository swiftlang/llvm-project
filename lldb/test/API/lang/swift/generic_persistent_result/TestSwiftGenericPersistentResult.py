import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftGenericPersistentResult(TestBase):
    def continue_to(self, process, function_name):
        process.Continue()
        frame = process.GetSelectedThread().GetSelectedFrame()
        self.assertIn(function_name, frame.GetFunctionName())

    @swiftTest
    @requireNotEmbeddedSwift
    def test_generic_persistent_result(self):
        self.build()
        target, process, _, _ = lldbutil.run_to_source_breakpoint(
            self, "break in generic method", lldb.SBFileSpec("main.swift"))
        lldbutil.run_break_set_by_source_regexp(
            self, "break in other generic method")
        lldbutil.run_break_set_by_source_regexp(
            self, "break in non-generic function")

        self.expect("expression --bind-generic-types false -- t",
                    substrs=["$R0", "42"])
        self.expect("expression --bind-generic-types false -- [t]",
                    substrs=["$R1", "42"])
        frame = process.GetSelectedThread().GetSelectedFrame()
        options = lldb.SBExpressionOptions()
        options.SetLanguage(lldb.eLanguageTypeSwift)
        value = frame.EvaluateExpression("[t]", options)
        self.assertSuccess(value.GetError())
        self.assertIn("42", value.GetObjectDescription())

        # Here τ_0_0 is String, not the Int $R0 was created with.
        self.continue_to(process, "OtherGeneric")
        self.expect("expression --bind-generic-types false -- u",
                    substrs=["String", "hello"])
        self.expect("expression $R0", substrs=["Int", "42"])

        self.continue_to(process, "nonGeneric")
        self.expect("expression $R0", substrs=["Int", "42"])
        self.assertIn("42", value.GetObjectDescription())
        # Force the fallback taken when the target's Swift runtime doesn't
        # provide stringForPrintObject(_:mangledTypeName:).
        self.runCmd(
            "settings set target.experimental.swift-use-context-free-po false")
        self.expect("po $R1", substrs=["42"])
