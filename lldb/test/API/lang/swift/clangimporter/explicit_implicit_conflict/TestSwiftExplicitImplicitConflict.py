import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftExplicitImplicitConflict(TestBase):
    NO_DEBUG_INFO_TESTCASE = True

    def import_conflicting_modules(self):
        self.build()
        self.runCmd(
            'settings set symbols.clang-modules-cache-path "%s"'
            % self.getBuildArtifact("lldb-module-cache")
        )
        _, process, _, _ = lldbutil.run_to_source_breakpoint(
            self,
            "break implicit",
            lldb.SBFileSpec("Implicit.swift"),
            extra_images=["Implicit"],
        )
        # Implicit is built without explicit modules, so its context
        # populates LLDB's module cache with implicitly built PCMs.
        self.expect("expression z", substrs=["z = 1"])
        lldbutil.continue_to_source_breakpoint(
            self, process, "break lib", lldb.SBFileSpec("Lib.swift")
        )
        # Lib's context loads Avail from an explicitly built PCM. Y is
        # not in Lib's explicit module map, so the implicitly built Y
        # from LLDB's module cache is used, which depends on a
        # different Avail PCM.
        self.expect(
            "expression -- import Y",
            error=True,
            substrs=["module 'Avail' is defined in both"],
        )
        self.expect(
            "expression -- import Z",
            error=True,
            substrs=["module 'Avail' is defined in both"],
        )
        # Deserializing a declaration with a name Clang has not seen yet
        # visits the identifier tables of all loaded module files.
        self.expect("type lookup Shared", substrs=["struct Shared"])
        # A failed module load can leave Clang's ASTReader inconsistent,
        # so later expressions must not reuse that compiler instance.
        self.expect("expression s", substrs=["a = 3"])
        self.expect("frame variable s", substrs=["a = 3"])

    @requireNotEmbeddedSwift
    @skipIf(setting=("symbols.use-swift-clangimporter", "false"))
    @skipUnlessDarwin
    @swiftClangImporterTest
    def test(self):
        self.import_conflicting_modules()

    @requireNotEmbeddedSwift
    @skipIf(setting=("symbols.use-swift-clangimporter", "false"))
    @skipUnlessDarwin
    @swiftClangImporterTest
    def test_implicit_module_loader(self):
        """Same as above, but without disabling implicit imports during
        type reconstruction."""
        setting = "target.experimental.swift-allow-implicit-module-loader"
        self.runCmd("settings set %s true" % setting)
        self.addTearDownHook(lambda: self.runCmd("settings clear %s" % setting))
        self.import_conflicting_modules()
