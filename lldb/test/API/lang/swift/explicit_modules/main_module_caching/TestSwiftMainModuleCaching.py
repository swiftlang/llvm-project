"""
Test that a compile-cached build of the main module is picked up out of the
CAS, and that losing the CAS falls back to a non-explicit module build rather
than failing the expression.
"""

import shutil

import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftMainModuleCaching(TestBase):
    NO_DEBUG_INFO_TESTCASE = True
    # One build per test: the CAS lives in the build directory, and the test
    # below that deletes it must not take the CAS out from under its sibling.
    SHARED_BUILD_TESTCASE = False

    def run_to_expression(self):
        """Stop in the inferior and evaluate an expression, which is what
        creates the SwiftASTContext under test. Returns the types log
        describing how that context was configured."""
        log = self.getBuildArtifact("types.log")
        self.runCmd('settings set symbols.cas-path "%s"' % self.getBuildArtifact("cas"))
        # Force loading from interface to simulate no binary module available.
        self.runCmd("settings set symbols.swift-module-loading-mode prefer-interface")
        self.runCmd('log enable lldb types -f "%s"' % log)
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        self.expect("expression 1", substrs=["(Int) $", " = 1"])
        return log

    @skipIf(setting=("symbols.use-swift-clangimporter", "false"))
    @requireNotEmbeddedSwift
    @skipUnlessDarwin
    @swiftTest
    def test_main_module_from_cas(self):
        self.build()
        self.filecheck_log(
            self.run_to_expression(), __file__, "--check-prefixes=CHECK,CAS-LOAD"
        )

    @skipIf(setting=("symbols.use-swift-clangimporter", "false"))
    @requireNotEmbeddedSwift
    @skipUnlessDarwin
    @swiftTest
    def test_missing_cas(self):
        self.build()
        # An object store stays cached for the lifetime of the process once it
        # has been opened, so the CAS has to be gone before the first target
        # exists for this to be a miss at all.
        shutil.rmtree(self.getBuildArtifact("cas"))
        self.filecheck_log(
            self.run_to_expression(), __file__, "--check-prefixes=CHECK,CAS-MISS"
        )


# CHECK:            ConfigureDefaultCASStorage() -- Bound default CAS at path
# CAS-LOAD:         DiscoverExplicitMainModule() -- Discovered main module llvmcas://
# CAS-MISS:         DiscoverExplicitMainModule() -- Could not load main module {{.*}}llvmcas://{{.*}}No such file or directory
# CAS-LOAD:         LogConfiguration() --   Explicit modules : true
# CAS-MISS:         LogConfiguration() --   Explicit modules : false
# CHECK:            LogConfiguration() --   Extra clang arguments
# CAS-LOAD-COUNT-1: LogConfiguration() --     -triple
# CAS-LOAD:         LogConfiguration() --     -fmodule-file-cache-key
