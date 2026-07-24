# Auto-generated hammer shard for rdar://182785830 (flaky inline Data -> "197121
# bytes" instead of "3 bytes"). Independent, single-iteration copy of the
# explicit_modules part02 test_import check, so lit runs many in parallel to
# amplify attempts AND concurrent/CPU-starved load without any single test
# exceeding the 600s per-test timeout. Delete before merging.
import os
import shutil
import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftDataHammer14(lldbtest.TestBase):

    @skipEmbeddedSwift
    @swiftTest
    @skipUnlessDarwin
    def test_import(self):
        """Repeat (via sharding) the flaky inline-Data resolution check."""
        mod_cache = self.getBuildArtifact("my-clang-modules-cache")
        if os.path.isdir(mod_cache):
            shutil.rmtree(mod_cache)
        self.runCmd('settings set symbols.clang-modules-cache-path "%s"'
                    % mod_cache)

        self.build()
        target, process, thread, bkpt = lldbutil.run_to_source_breakpoint(
            self, 'Set breakpoint here', lldb.SBFileSpec('main.swift'))

        self.expect('expression Data([1, 2, 3])', error=True)
        self.expect("expression import Foundation")
        self.expect('expression Data([1, 2, 3])', substrs=["3 bytes"])
