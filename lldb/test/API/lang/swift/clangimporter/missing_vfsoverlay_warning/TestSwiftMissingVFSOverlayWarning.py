"""
Tests that a VFS overlay which is referenced by the serialized Clang importer
options but no longer exists is reported as a warning.
"""
import os
import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftMissingVFSOverlayWarning(TestBase):

    NO_DEBUG_INFO_TESTCASE = True

    @expectedFailureAll(oslist=["windows"])
    @swiftTest
    def test(self):
        overlay = self.getBuildArtifact("overlay.yaml")
        with open(overlay, "w") as f:
            f.write("{ 'version': 0, 'roots': [] }\n")
        self.build()
        os.remove(overlay)

        listener = lldbutil.start_listening_from(
            self.dbg.GetBroadcaster(), lldb.SBDebugger.eBroadcastBitWarning
        )
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        # Creating the Swift AST context is what emits the warning, and a
        # missing overlay must not stop the expression from succeeding.
        self.expect("expression 1", substrs=["1"])

        warnings = []
        event = lldb.SBEvent()
        while listener.GetNextEvent(event):
            diagnostic = lldb.SBDebugger.GetDiagnosticFromEvent(event)
            warnings.append(diagnostic.GetValueForKey("message").GetStringValue(4096))
        self.assertTrue(
            any("Ignoring missing VFS file" in w and overlay in w for w in warnings),
            "no missing VFS overlay warning in %s" % warnings,
        )
