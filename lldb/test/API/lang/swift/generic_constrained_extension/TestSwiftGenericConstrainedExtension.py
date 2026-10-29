"""
Test expression evaluation in a constrained extension of a generic type, where
locals have types that are only valid under the extension's requirements:
associated types of the required protocols, or generic types with requirements
of their own.
"""

import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftGenericConstrainedExtension(TestBase):
    NO_DEBUG_INFO_TESTCASE = True

    def continue_to(self, breakpoint_text):
        lldbutil.continue_to_source_breakpoint(
            self, self.process(), breakpoint_text, lldb.SBFileSpec("main.swift")
        )

    def expect_unbound_declined(self, expr):
        self.expect(
            "expression --bind-generic-types false -- " + expr,
            error=True,
            substrs=["Could not evaluate the expression without binding generic types"],
        )

    def check_local(self):
        # Every local is injected into the expression, including ones it
        # doesn't name.
        self.expect_expr("1", result_value="1")
        self.expect_expr("local", result_value="42")
        self.expect("po local", substrs=["42"])
        # The locals' types are not expressible without the extension's
        # requirements.
        self.expect_unbound_declined("1")
        self.expect_expr("local", result_value="42")

    def check_wrapper(self):
        # Boxing wrapper into Any needs its U: P conformance.
        self.expect("expression wrapper as Any", substrs=["Wrapper<a.S>"])
        self.expect_expr("wrapper.u.make()", result_value="42")
        self.expect(
            "expression --bind-generic-types true -- wrapper as Any",
            substrs=["Wrapper<a.S>"],
        )

    @swiftTest
    @requireNotEmbeddedSwift
    def test(self):
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break in class extension", lldb.SBFileSpec("main.swift")
        )
        self.check_local()
        self.expect_expr("box.v", result_value="42")

        self.continue_to("break in array extension")
        self.check_local()

        self.continue_to("break in where clause")
        self.check_wrapper()
        self.expect_unbound_declined("wrapper as Any")

        self.continue_to("break in constrained type")
        self.check_wrapper()

        self.continue_to("break in pack wrapper method")
        self.expect("expression packWrapper as Any", substrs=["PackWrapper<Pack{a.S}>"])
        self.expect_unbound_declined("packWrapper as Any")

        self.continue_to("break in wrapper method")
        self.check_wrapper()
        self.expect_unbound_declined("wrapper as Any")
        # The expression fallback of po goes through the same path.
        self.runCmd("settings set target.experimental.swift-use-context-free-po false")
        self.expect("po wrapper", substrs=["<Wrapper<S>: 0x"])
