"""
Test that expressions fail with an error, rather than crash, when the Swift
standard library fails to load as a dependency of another module.
"""

import os
import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftStdlibImportFailure(TestBase):
    NO_DEBUG_INFO_TESTCASE = True

    def setUp(self):
        TestBase.setUp(self)
        self.build()
        # A module search path takes precedence over the resource directory,
        # so this shadows the real stdlib.
        bogus = self.getBuildArtifact("bogus")
        os.makedirs(bogus, exist_ok=True)
        with open(os.path.join(bogus, "Swift.swiftmodule"), "w") as f:
            f.write("not a swift module")
        self.runCmd("settings set target.swift-module-search-paths " + bogus)
        self.addTearDownHook(
            lambda: self.runCmd("settings clear target.swift-module-search-paths")
        )
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )

    @swiftTest
    @skipEmbeddedSwift
    def test_expr(self):
        self.expect(
            "expression c.x",
            error=True,
            substrs=["the Swift standard library failed to load"],
        )

    @swiftTest
    @skipEmbeddedSwift
    def test_po(self):
        # po falls back to p, which doesn't need the expression evaluator.
        self.expect("po c", substrs=["x = 42"])
        self.expect("frame variable c.x", substrs=["42"])
