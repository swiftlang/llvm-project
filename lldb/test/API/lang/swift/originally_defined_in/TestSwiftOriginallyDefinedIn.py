import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftOriginallyDefinedIn(lldbtest.TestBase):
    @swiftTest
    def test(self):
        """Test that types with the @_originallyDefinedIn attribute can still be found in metadata"""

        self.build()
        self.runCmd("setting set symbols.swift-enable-ast-context false")
        filespec = lldb.SBFileSpec("main.swift")
        target, process, thread, breakpoint1 = lldbutil.run_to_source_breakpoint(
            self, "break here", filespec
        )
        self.expect("frame variable a", substrs=["a.A", "a = (i = 10)"])
        self.expect("frame variable b", substrs=["a.Alias", "b = (i = 20)"])
        self.expect("frame variable d", substrs=["a.C.D", "d = (i = 30)"])
        self.expect("frame variable e", substrs=["a.E<a.Prop>", "e = (i = 40)"])
