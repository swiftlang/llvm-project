import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import TestBase
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftEmbeddedCurrentTask(TestBase):
    @skipUnlessDarwin
    @swiftTest
    @skipUnlessEmbeddedSwift
    def test(self):
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        self.expect("language swift task info", substrs=["address ="])
