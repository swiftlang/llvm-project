import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftResiliencePrivateMethod(TestBase):
    @requireNotEmbeddedSwift
    @requireSwiftObjCInterop
    @swiftClangImporterTest
    def test(self):
        self.build()
        target, process, thread, bkpt = lldbutil.run_to_source_breakpoint(
            self, 'break here', lldb.SBFileSpec('main.swift'))

        self.expect("expression priv", substrs=["a.C"])
