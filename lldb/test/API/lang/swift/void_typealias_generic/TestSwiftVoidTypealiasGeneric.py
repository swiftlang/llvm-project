import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftVoidTypealiasGeneric(TestBase):
    @swiftTest
    def test(self):
        self.build()
        self.runCmd("settings set symbols.swift-enable-ast-context false")
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )

        frame = self.frame()

        box_of_void = frame.FindVariable("boxOfVoid")
        lldbutil.check_variable(self, box_of_void, num_children=2)
        tag = box_of_void.GetChildMemberWithName("tag")
        lldbutil.check_variable(self, tag, value="7")

        nested_void = frame.FindVariable("nestedVoid")
        lldbutil.check_variable(self, nested_void, num_children=2)
        snd = nested_void.GetChildAtIndex(1)
        lldbutil.check_variable(self, snd, value="42")

