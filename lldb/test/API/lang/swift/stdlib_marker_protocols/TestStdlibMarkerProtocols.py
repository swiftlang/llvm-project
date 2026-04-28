import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestStdlibMarkerProtocols(TestBase):
    @swiftTest
    @skipEmbeddedSwift
    def test(self):
        self.build()
        self.runCmd("settings set symbols.swift-enable-ast-context false")
        target, process, thread, bkpt = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        frame = thread.frames[0]

        lldbutil.check_variable(
            self, frame.FindVariable("sendable"), use_dynamic=True, value="1"
        )
        lldbutil.check_variable(
            self,
            frame.FindVariable("bitwiseCopyable"),
            use_dynamic=True,
            value="2",
        )
        lldbutil.check_variable(
            self,
            frame.FindVariable("sendableAndBitwise"),
            use_dynamic=True,
            value="3",
        )
        lldbutil.check_variable(
            self,
            frame.FindVariable("realAndSendable"),
            use_dynamic=True,
            value="4",
        )
