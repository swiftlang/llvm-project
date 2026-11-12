import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil

SYNCHRONIZATION = "libswiftSynchronization.dylib"


class TestCase(lldbtest.TestBase):
    SHARED_BUILD_TESTCASE = False
    NO_DEBUG_INFO_TESTCASE = True

    def _is_loaded(self, target):
        names = [m.GetFileSpec().GetFilename() for m in target.module_iter()]
        return SYNCHRONIZATION in names

    def _setup_log(self):
        # Without reflection metadata for Mutex, fail instead of falling back
        # to the compiler, which shows no fields for @_rawLayout storage.
        self.runCmd("settings set symbols.swift-typesystem-compiler-fallback false")
        log = self.getBuildArtifact("types.log")
        self.runCmd(f'log enable -v lldb types -f "{log}"')
        return log

    # The reflection metadata of the @_rawLayout storage of Mutex is emitted
    # by the Synchronization library in macOS 27.
    @skipIf(macos_version=["<", "27.0"])
    @skipUnlessDarwin
    @requireNotEmbeddedSwift
    @swiftClangImporterTest
    @swiftTest
    def test(self):
        """Test that LLDB reads the reflection metadata of a shared cache Swift
        library that isn't linked and loaded into the process."""

        self.build(dictionary={"SWIFT_SOURCES": "main.swift"})
        target, _, _, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        self.assertFalse(
            self._is_loaded(target), "the program should not load Synchronization"
        )
        log = self._setup_log()

        self.expect(
            "frame variable mutex",
            substrs=["(Synchronization.Mutex<Int>) mutex = {", "_rawLayout = 16"],
        )
        self.expect(
            "frame variable pair",
            substrs=["(Synchronization.Mutex<a.Pair>) pair = {", "a = 1", "b = 3"],
        )

        # The library's image is read without adding it to the target.
        self.assertFalse(self._is_loaded(target))
        self.filecheck_log(log, __file__, "--check-prefix=CHECK-MAIN")
        # The image is registered once, for both instantiations of Mutex.
        # CHECK-MAIN: Added reflection metadata of "/usr/lib/swift/libswiftSynchronization.dylib", which the process has not loaded, from its shared cache
        # CHECK-MAIN-NOT: Added reflection metadata of

    @skipIf(macos_version=["<", "27.0"])
    @skipUnlessDarwin
    @requireNotEmbeddedSwift
    @swiftClangImporterTest
    @swiftTest
    def test_load_later(self):
        """Test that a shared cache Swift library registered isn't registered
        again when the process loads it."""

        self.build(dictionary={"SWIFT_SOURCES": "dlopen.swift"})
        dlopen_spec = lldb.SBFileSpec("dlopen.swift")
        # The process has to load the same image as the one in the shared
        # cache. Drop the library-path variables the test harness injects into
        # the inferior environment, which would make dlopen load the
        # Synchronization library of the Swift build instead.
        target = self.dbg.CreateTarget(self.getBuildArtifact("a.out"))
        self.assertTrue(target, lldbtest.VALID_TARGET)
        for var in (
            "DYLD_LIBRARY_PATH",
            "LD_LIBRARY_PATH",
            "SIMCTL_CHILD_DYLD_LIBRARY_PATH",
        ):
            self.runCmd(f"settings remove target.env-vars {var}", check=False)
        bkpt = target.BreakpointCreateBySourceRegex("before load", dlopen_spec)
        self.assertGreater(bkpt.GetNumLocations(), 0)
        _, process, _, _ = lldbutil.run_to_breakpoint_do_run(self, target, bkpt)
        self.assertFalse(
            self._is_loaded(target), "the program should not load Synchronization yet"
        )
        log = self._setup_log()

        self.expect("frame variable mutex", substrs=["_rawLayout = 16"])

        after_bkpt = target.BreakpointCreateBySourceRegex("after load", dlopen_spec)
        lldbutil.continue_to_breakpoint(process, after_bkpt)
        self.assertTrue(
            self._is_loaded(target), "the program should have loaded Synchronization"
        )

        self.expect("frame variable mutex", substrs=["_rawLayout = 16"])
        self.filecheck_log(log, __file__, "--check-prefix=CHECK-LOAD")
        # CHECK-LOAD: Added reflection metadata of "/usr/lib/swift/libswiftSynchronization.dylib", which the process has not loaded, from its shared cache
        # CHECK-LOAD-NOT: reflection metadata in "libswiftSynchronization.dylib"
        # CHECK-LOAD-NOT: Added reflection metadata of
