"""
Test that LLDB is oblivious if the SDK the program was built against doesn't exist.
"""

import lldb
import lldbsuite.test.lldbutil as lldbutil
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *

class TestSwiftMissingSDK(TestBase):
    NO_DEBUG_INFO_TESTCASE = True

    def setUp(self):
        # Call super's setUp().
        TestBase.setUp(self)

    @skipEmbeddedSwift
    @swiftTest
    @skipIfDarwinEmbedded # swift crash inspecting swift stdlib with little other swift loaded <rdar://problem/55079456>
    def testMissingSDK(self):
        self.build()
        fakesdk = self.getBuildArtifact("fakesdk")
        # On Windows fakesdk is a directory junction. os.rmdir removes the
        # junction without touching the real SDK. Elsewhere it is a symlink.
        if os.path.isdir(fakesdk) and not os.path.islink(fakesdk):
            os.rmdir(fakesdk)
        else:
            os.unlink(fakesdk)
        lldbutil.run_to_source_breakpoint(self, 'break here',
                                          lldb.SBFileSpec('main.swift'))
        self.expect("expression message", VARIABLES_DISPLAYED_CORRECTLY,
                    substrs = ["Hello"])

