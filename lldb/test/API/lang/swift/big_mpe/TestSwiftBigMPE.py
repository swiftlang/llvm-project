import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftBigMPE(TestBase):
    @swiftTest
    def test(self):
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )

        self.expect("frame variable a1", substrs=["a.Request", "a1 = a1"])
        self.expect("frame variable a2", substrs=[".Request", "a2 = a2"])
        self.expect("frame variable a3", substrs=[".Request", "a3 = a3"])
        self.expect("frame variable a4", substrs=[".Request", "a4 = a4"])
        self.expect("frame variable a5", substrs=[".Request", "a5 = a5"])
        self.expect(
            "frame variable a6_item1",
            substrs=[".Request", "a6_item1 = a6", "a6 = item1"],
        )
        self.expect(
            "frame variable a6_item2",
            substrs=[".Request", "a6_item2 = a6", "a6 = item2"],
        )
        self.expect(
            "frame variable a7_item1",
            substrs=[".Request", "a7_item1 = a7", "a7 = item1"],
        )
        self.expect(
            "frame variable a7_item2",
            substrs=[".Request", "a7_item2 = a7", "a7 = item2"],
        )
