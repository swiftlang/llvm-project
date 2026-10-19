import os

import lldb
from lldbsuite.test.decorators import skipEmbeddedSwift, skipUnlessDarwin, swiftTest
from lldbsuite.test.lldbtest import TestBase
from lldbsuite.test import lldbutil


class TestSwiftMacroPaths(TestBase):
    NO_DEBUG_INFO_TESTCASE = True
    SHARED_BUILD_TESTCASE = False

    def check_macro(self, kind, interactive=False, relative=False, debug_module=False):
        # Match the plugin server to the SwiftSyntax used to build the plugin.
        server = "swift-plugin-server".join(self.getCompiler().rsplit("clang", 1))
        if not os.path.exists(server):
            server = "swift".join(server.rsplit("llvm", 1))
        self.assertTrue(os.path.exists(server), server)
        build = self.getBuildDir()
        library = self.getBuildArtifact("libMacroImpl.dylib")
        executable = self.getBuildArtifact("MacroPlugin")
        flags = {
            "path": f"-plugin-path {build}",
            "external": f"-external-plugin-path {build}#{server}",
            "library": f"-load-plugin-library {library}",
            "executable": f"-load-plugin-executable {executable}#MacroImpl",
            "resolved": (
                f"-Xfrontend -load-resolved-plugin -Xfrontend {library}#{server}#MacroImpl"
            ),
            "resolved_executable": (
                f"-Xfrontend -load-resolved-plugin -Xfrontend '#{executable}#MacroImpl'"
            ),
        }
        options = {"PLUGIN_FLAGS": flags[kind], "PLUGIN_SERVER": server}
        build_prefix = "." if relative else "/__lldb_macro_build__"
        options["BUILD_PREFIX"] = build_prefix
        if kind in ("executable", "resolved_executable"):
            options["PLUGIN_IMPL"] = "MacroPlugin"
        if interactive:
            options.update(MAIN_IMPORT_FLAGS="", MAIN_PLUGIN_FLAGS="")
        if debug_module:
            options["MAIN_DEBUG_FLAGS"] = (
                "-debug-module-path " + self.getBuildArtifact("a.swiftmodule")
            )
        self.build(dictionary=options)

        # Both the plugin and server must be remapped before LLDB checks that
        # they exist or tries to launch them. Also exercise Bazel-style relative
        # paths and plugin server overrides matched against the local path.
        self.runCmd(
            'settings set target.source-map "%s" "%s" '
            '/__lldb_macro_toolchain__ "%s"'
            % (build_prefix, build, os.path.dirname(server))
        )
        self.runCmd('settings set target.swift-module-search-paths "%s"' % build)
        if kind in ("path", "library"):
            self.runCmd(
                'settings set target.experimental.swift-plugin-server-for-path "%s=%s"'
                % (build, server)
            )
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        if interactive:
            self.expect("expression -- import Macro")
        self.expect(
            "expression -- #stringify(value)", substrs=["0 = 42", '1 = "value"']
        )

    @skipEmbeddedSwift
    @skipUnlessDarwin
    @swiftTest
    def test_plugin_path(self):
        self.check_macro("path")

    @skipEmbeddedSwift
    @skipUnlessDarwin
    @swiftTest
    def test_external_plugin_path(self):
        self.check_macro("external")

    @skipEmbeddedSwift
    @skipUnlessDarwin
    @swiftTest
    def test_plugin_library(self):
        self.check_macro("library")

    @skipEmbeddedSwift
    @skipUnlessDarwin
    @swiftTest
    def test_plugin_executable(self):
        self.check_macro("executable")

    @skipEmbeddedSwift
    @skipUnlessDarwin
    @swiftTest
    def test_resolved_plugin(self):
        self.check_macro("resolved")

    @skipEmbeddedSwift
    @skipUnlessDarwin
    @swiftTest
    def test_resolved_plugin_executable(self):
        self.check_macro("resolved_executable")

    @skipEmbeddedSwift
    @skipUnlessDarwin
    @swiftTest
    def test_interactive_import(self):
        self.check_macro("external", interactive=True)

    @skipEmbeddedSwift
    @skipUnlessDarwin
    @swiftTest
    def test_relative_plugin_path(self):
        self.check_macro("path", relative=True)

    @skipEmbeddedSwift
    @skipUnlessDarwin
    @swiftTest
    def test_debug_module_path(self):
        self.check_macro("resolved", debug_module=True)
