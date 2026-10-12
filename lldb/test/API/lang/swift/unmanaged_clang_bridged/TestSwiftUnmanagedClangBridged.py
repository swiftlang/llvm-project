import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftUnmanagedClangBridged(TestBase):
    @swiftTest
    def test(self):
        self.build()
        target, process, thread, bkpt = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        self.runCmd("settings set symbols.swift-enable-ast-context false")

        frame = thread.frames[0]

        someU = frame.FindVariable("someU")
        lldbutil.check_variable(
            self, someU, typename="Swift.Unmanaged<Foo.MyBridgedRef>"
        )
        self.assertEqual(
            someU.GetChildMemberWithName("_value").GetValueAsUnsigned(), 0xdeadbeef
        )

        optU = frame.FindVariable("optU")
        lldbutil.check_variable(
            self,
            optU,
            typename="Swift.Optional<Swift.Unmanaged<Foo.MyBridgedRef>>",
            value="some",
        )
        self.assertEqual(
            optU.GetChildMemberWithName("_value").GetValueAsUnsigned(), 0xdeadbeef
        )

        nilU = frame.FindVariable("nilU")
        lldbutil.check_variable(
            self,
            nilU,
            typename="Swift.Optional<Swift.Unmanaged<Foo.MyBridgedRef>>",
            summary="nil",
        )
