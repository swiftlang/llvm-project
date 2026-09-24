import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil


class TestCase(lldbtest.TestBase):
    @skipEmbeddedSwift
    @swiftTest
    def test(self):
        """Test that type(of:) works on variables of existential type."""

        self.build()
        filespec = lldb.SBFileSpec("main.swift")
        lldbutil.run_to_source_breakpoint(self, "break here", filespec)

        dynamic = lldb.SBExpressionOptions()
        dynamic.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)

        static = lldb.SBExpressionOptions()
        static.SetFetchDynamicValue(lldb.eNoDynamicValues)

        # With dynamic type resolution the existential is replaced by the
        # concrete type it holds.
        self.expect_expr("type(of: single)", result_summary="a.S", options=dynamic)
        self.expect_expr("type(of: composition)", result_summary="a.S", options=dynamic)
        self.expect_expr("type(of: inherited)", result_summary="a.S", options=dynamic)
        self.expect_expr("type(of: anything)", result_summary="Int", options=dynamic)

        # Without dynamic type resolution the static existential type is used.
        self.expect_expr("type(of: single)", result_type="a.P1.Type", options=static)
        self.expect_expr(
            "type(of: composition)", result_type="a.P1 & a.P2.Type", options=static
        )
        self.expect_expr(
            "type(of: inherited)", result_type="a.Composed.Type", options=static
        )
