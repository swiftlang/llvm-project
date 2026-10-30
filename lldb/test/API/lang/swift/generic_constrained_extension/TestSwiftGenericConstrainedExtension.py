"""
Test expression evaluation in a constrained extension of a generic type, where
locals have types that use associated types of the extension's requirements.
"""

import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftGenericConstrainedExtension(TestBase):
    def check(self, breakpoint_text):
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, breakpoint_text, lldb.SBFileSpec("main.swift")
        )
        # Every local is injected into the expression, including ones it
        # doesn't name.
        self.expect_expr("1", result_value="1")
        self.expect_expr("local", result_value="42")
        self.expect("po local", substrs=["42"])
        # The locals' types are not expressible without the extension's
        # requirements.
        self.expect(
            "expression --bind-generic-types false -- 1",
            error=True,
            substrs=[
                "Could not evaluate the expression without binding generic types"
            ],
        )
        self.expect_expr("local", result_value="42")

    @swiftTest
    @requireNotEmbeddedSwift
    @skipIf(debug_info=["pdb"])
    def test_class_extension(self):
        self.check("break in class extension")
        self.expect_expr("box.v", result_value="42")

    @swiftTest
    @requireNotEmbeddedSwift
    @skipIf(debug_info=["pdb"])
    def test_array_extension(self):
        self.check("break in array extension")
