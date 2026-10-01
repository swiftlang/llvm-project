import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestDesignatedInitializerSelf(TestBase):

    # rdar://185128962 (Embedded Swift: po falls back to p, so object-description output is unavailable)
    @skipEmbeddedSwift
    @swiftTest
    def test(self):
        """Test that self can be printed in a convenience init after self.init()"""
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        self.expect('po self', substrs=['<C: 0x'])
