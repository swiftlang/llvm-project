"""
DWARF and reflection metadata agree on the layout of a regular Swift program's
types. Every type bottoms out in a builtin, so this needs the DWARF side to find
the builtins of a regular Swift module, whose mangled names use the $s flavor.

The comparison only exists in an asserts-enabled LLDB; without one this test
compares nothing.
"""

import lldb
import lldbsuite.test.lldbutil as lldbutil
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *


class TestSwiftDWARFValidationBuiltins(TestBase):
    NO_DEBUG_INFO_TESTCASE = True

    @swiftTest
    @requireNotEmbeddedSwift
    def test(self):
        self.build()
        log = self.getBuildArtifact("types.log")
        self.runCmd("settings set testing.swift-validate-typesystem-dwarf report")
        self.runCmd('log enable -f "%s" lldb types' % log)
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        self.expect("frame variable foo bar", substrs=["x = 1", "a = 1"])
        self.runCmd("log disable lldb types")
        with open(log) as f:
            disagreements = [l for l in f if "disagree on the layout of" in l]
        self.assertEqual(disagreements, [])
