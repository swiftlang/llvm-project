"""
Tests that a warning is printed when a Swift AST context is initialized for
code compiled by a different Swift compiler than the one embedded in LLDB.
"""
import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil

MISMATCH = "was compiled with a different Swift compiler"


class TestSwiftToolchainMismatch(TestBase):

    NO_DEBUG_INFO_TESTCASE = True

    def setUp(self):
        TestBase.setUp(self)
        self.listener = lldbutil.start_listening_from(
            self.dbg.GetBroadcaster(), lldb.SBDebugger.eBroadcastBitWarning
        )

    def warnings_from_expression(self, exe, source):
        """Evaluate an expression in exe and return the warnings it produced"""
        target, process, _, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec(source), exe_name=exe
        )
        # The warning comes out of setting up the Swift AST context, so the
        # expression only has to be evaluated, not to succeed.
        self.runCmd("expression 1", check=False)

        messages = []
        event = lldb.SBEvent()
        while self.listener.GetNextEvent(event):
            diagnostic = lldb.SBDebugger.GetDiagnosticFromEvent(event)
            messages.append(diagnostic.GetValueForKey("message").GetStringValue(4096))

        # Leave the debugger clean for the next executable.
        process.Kill()
        self.dbg.DeleteTarget(target)
        return messages

    @skipEmbeddedSwift
    @swiftTest
    def test_toolchain_mismatch(self):
        self.build()

        warnings = self.warnings_from_expression("mismatch.out", "main.swift")
        self.assertTrue(
            any(MISMATCH in w and "9999.8.7.6" in w for w in warnings),
            "no toolchain mismatch warning in %s" % warnings,
        )

        # A binary from the matching compiler must not warn.
        warnings = self.warnings_from_expression("good.out", "main.swift")
        self.assertFalse(
            any(MISMATCH in w for w in warnings),
            "unexpected toolchain mismatch warning in %s" % warnings,
        )
