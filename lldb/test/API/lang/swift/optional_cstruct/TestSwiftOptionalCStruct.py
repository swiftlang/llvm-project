import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftOptionalCStruct(TestBase):
    @swiftTest
    @skipEmbeddedSwiftOnWindows
    def test(self):
        self.build()
        self.runCmd("settings set symbols.swift-enable-ast-context false")
        target, process, thread, bkpt = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )

        frame = thread.frames[0]
        variable = frame.FindVariable("variable")
        lldbutil.check_variable(
            self, variable, value="some", typename="Swift.Optional<Foo.Foo>"
        )

        val = variable.GetChildMemberWithName("val")
        self.assertTrue(val.IsValid(), "val child is valid")
