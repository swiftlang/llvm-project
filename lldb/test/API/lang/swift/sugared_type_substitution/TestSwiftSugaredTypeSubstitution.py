import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil
from lldbsuite.test.lldbtest import ValueCheck

class TestCase(lldbtest.TestBase):
    @skipEmbeddedSwiftOnWindows
    @swiftTest
    def test(self):
        """Canonicalizing a sugared type must yield a canonical mangled name"""
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, 'break here', lldb.SBFileSpec('main.swift'))

        # Desugaring the [[Int]] in the key path's value type used to produce
        # a Type(Type(...)) demangle tree, which defeated the remangler's
        # structural substitution matching. The resulting mangled name was not
        # canonical, which tripped an assertion in CompilerType.
        self.expect_var_path("kp", type="WritableKeyPath<S, [[Int]]>")
        self.expect_var_path(
            "s",
            children=[
                ValueCheck(
                    children=[
                        ValueCheck(
                            summary="2 values",
                            children=[
                                ValueCheck(name="[0]", value="1"),
                                ValueCheck(name="[1]", value="2"),
                            ],
                        ),
                    ],
                )
            ],
        )
