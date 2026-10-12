import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftUnmanagedFoundationType(TestBase):
    @swiftTest
    @skipUnlessFoundation
    def test(self):
        """Inspect Unmanaged of a Clang-imported reference type without the AST context."""
        self.build()
        target, process, thread, bkpt = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        self.runCmd("settings set symbols.swift-enable-ast-context false")

        frame = thread.frames[0]

        unmanaged = frame.FindVariable("unmanaged")
        lldbutil.check_variable(
            self, unmanaged, typename="Swift.Unmanaged<CoreFoundation.CFErrorRef>"
        )
        self.assertNotEqual(
            unmanaged.GetChildMemberWithName("_value").GetValueAsUnsigned(), 0
        )

        optUnmanaged = frame.FindVariable("optUnmanaged")
        lldbutil.check_variable(
            self,
            optUnmanaged,
            typename="Swift.Optional<Swift.Unmanaged<CoreFoundation.CFErrorRef>>",
            value="some",
        )
        self.assertNotEqual(
            optUnmanaged.GetChildMemberWithName("_value").GetValueAsUnsigned(), 0
        )

        nilUnmanaged = frame.FindVariable("nilUnmanaged")
        lldbutil.check_variable(
            self,
            nilUnmanaged,
            typename="Swift.Optional<Swift.Unmanaged<CoreFoundation.CFErrorRef>>",
            summary="nil",
        )
