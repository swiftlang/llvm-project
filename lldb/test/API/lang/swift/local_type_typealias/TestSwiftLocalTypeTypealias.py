"""
A type declared inside a function carries that function's full signature in its
mangled name. When the signature names a Clang swift_newtype type alias, the
compiler preserves the alias in the reflection field descriptor.
"""
import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftLocalTypeTypealias(lldbtest.TestBase):
    @skipUnlessDarwin
    @skipEmbeddedSwift
    @swiftTest
    def test(self):
        self.build()

        self.runCmd("settings set symbols.swift-enable-ast-context false")

        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))

        variable = self.frame().FindVariable("variable")
        self.assertIn("LocalBox", variable.GetTypeName())
        lldbutil.check_variable(self, variable, num_children=1)
        slot = variable.GetChildMemberWithName("slot")
        lldbutil.check_variable(self, slot, value="7")
