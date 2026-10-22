"""
Test that Swift expression diagnostics are drawn onto the command line.
"""
import lldb
import lldbsuite.test.lldbpexpect as lldbpexpect
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *


class TestSwiftInlineDiagnostics(lldbpexpect.PExpectTest):
    NO_DEBUG_INFO_TESTCASE = True

    # The merged layout below is only produced by the command interpreter's
    # IOHandler, which knows how far the prompt indents the command. Driving
    # lldb through SBCommandInterpreter yields the unmerged rendering instead,
    # so this has to go through a pty.
    #
    # The rendering doesn't depend on how the expression was compiled or on
    # how its types were imported, so the embedded variant exercises nothing
    # new.
    @requireNotEmbeddedSwift
    @swiftTest
    def test_inline_diagnostics(self):
        self.build()
        # LANG decides whether the diagnostic is drawn with box drawing
        # characters or with ASCII; pin it so the expected output is stable.
        self.launch(
            executable=self.getBuildArtifact("a.out"),
            run_under=["env", "LANG=C"],
        )
        # The driver enables this at startup, but PExpectTest's setup clears
        # all settings back to their built-in defaults.
        self.expect("settings set show-inline-diagnostics true")
        self.expect("breakpoint set --name main", substrs=["Breakpoint 1"])
        self.expect("run", substrs=["stop reason = breakpoint 1"])

        # Both errors refer to the same line, so they share one underline row
        # and the earlier one is pushed down a line to make room:
        #
        #   (lldb) expr foo+bar
        #               ^~~ ^~~
        #               |   error: cannot find 'bar' in scope
        #               error: cannot find 'foo' in scope
        #
        # The indentation has to line up with the expression as the prompt
        # echoed it, so match it exactly. The leading newline anchors each
        # match to the start of a line, which a bare substring would not.
        self.expect(
            "expr foo+bar",
            substrs=[
                "\r\n            ^~~ ^~~",
                "\r\n            |   error: cannot find 'bar' in scope",
                "\r\n            error: cannot find 'foo' in scope",
            ],
        )
