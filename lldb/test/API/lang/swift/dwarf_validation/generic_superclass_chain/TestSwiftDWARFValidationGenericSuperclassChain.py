"""
DWARF and reflection metadata agree on the layout of a class whose superclass
is a specialization of a generic class that itself has a superclass.
"""

import lldb
import lldbsuite.test.lldbutil as lldbutil
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *


class TestSwiftDWARFValidationGenericSuperclassChain(TestBase):
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
        self.expect("frame variable leaf", substrs=["a = 1", "x = 7", "b = 2"])
        self.runCmd("log disable lldb types")
        with open(log) as f:
            disagreements = [l for l in f if "disagree on the layout of" in l]
        self.assertEqual(disagreements, [])
