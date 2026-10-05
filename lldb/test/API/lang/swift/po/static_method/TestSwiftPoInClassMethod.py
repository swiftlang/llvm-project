"""
Test that context-free po works when stopped in a class/static method.
"""

import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestCase(TestBase):
    @swiftTest
    @skipEmbeddedSwift
    def test(self):
        """Context-free po should not emit extension $__lldb_context when
        stopped in a static method, since the frame's method context is
        irrelevant to the context-free expression."""
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        log = self.getBuildArtifact("expr.log")
        self.runCmd(f"log enable lldb expr -f {log}")
        self.expect("po value", substrs=["instance of C"])
        self.filecheck_log(log, __file__)
        # CHECK-NOT: $__lldb_context
        # CHECK: stringForPrintObject(_:mangledTypeName:) succeeded
