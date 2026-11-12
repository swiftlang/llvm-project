import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil


class TestCase(lldbtest.TestBase):
    NO_DEBUG_INFO_TESTCASE = True

    @skipUnlessDarwin
    @requireNotEmbeddedSwift
    @swiftTest
    def test(self):
        """Test that a type from a statically linked module, which no image and
        no Swift library in the shared cache provides, is still found in the
        reflection metadata of the program."""

        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )

        # Fail instead of falling back to the compiler if the reflection
        # metadata of Point isn't found.
        self.runCmd("settings set symbols.swift-typesystem-compiler-fallback false")
        log = self.getBuildArtifact("types.log")
        self.runCmd(f'log enable -v lldb types -f "{log}"')

        self.expect(
            "frame variable point", substrs=["(Lib.Point) point = (x = 1, y = 2)"]
        )

        self.filecheck_log(log, __file__)
        # CHECK: No Swift library with reflection metadata for module "Lib" in the shared cache
        # CHECK-NOT: Added reflection metadata of
