"""Verify the Clang modulemaps shipped with LLDB.framework and the staged
LLDBRPC framework headers let clients `@import LLDB;` / `@import LLDBRPC;`
(and the Swift equivalents when building with C++ interop)."""

import os
import shutil
import subprocess

import lldb
from lldbsuite.test import configuration
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import TestBase


@skipUnlessDarwin
class FrameworkModulemapsTestCase(TestBase):
    NO_DEBUG_INFO_TESTCASE = True

    def _sdk_path(self):
        return subprocess.check_output(
            ["xcrun", "--show-sdk-path"], text=True
        ).strip()

    def _rpc_parked_headers_dir(self):
        return os.path.join(
            configuration.lldb_obj_root, "tools", "lldb-rpc", "ParkedHeaders"
        )

    def _stage_rpc_framework(self, dest):
        parked = self._rpc_parked_headers_dir()
        fw = os.path.join(dest, "LLDBRPC.framework")
        headers = os.path.join(fw, "Headers")
        modules = os.path.join(fw, "Modules")
        os.makedirs(headers)
        os.makedirs(modules)
        for name in os.listdir(os.path.join(parked, "Headers")):
            shutil.copy(os.path.join(parked, "Headers", name), headers)
        shutil.copy(
            os.path.join(parked, "Modules", "module.modulemap"), modules
        )
        return fw

    def _compile_objcxx_import(self, module_name, framework_parent_dir):
        src = self.getBuildArtifact("import_%s.mm" % module_name.lower())
        obj = self.getBuildArtifact("import_%s.o" % module_name.lower())
        cache = self.getBuildArtifact("modules-cache-%s" % module_name.lower())
        os.makedirs(cache, exist_ok=True)
        with open(src, "w") as f:
            f.write("@import %s;\n" % module_name)
        subprocess.check_call(
            [
                "xcrun",
                "clang++",
                "-x",
                "objective-c++",
                "-std=c++17",
                "-isysroot",
                self._sdk_path(),
                "-fmodules",
                "-fcxx-modules",
                "-fmodules-cache-path=" + cache,
                "-F",
                framework_parent_dir,
                "-c",
                src,
                "-o",
                obj,
            ]
        )
        self.assertTrue(os.path.isfile(obj))

    def _compile_swift_import(self, module_name, framework_parent_dir):
        swiftc = configuration.swiftCompiler
        if not swiftc or not os.path.isfile(swiftc):
            try:
                swiftc = subprocess.check_output(
                    ["xcrun", "-f", "swiftc"], text=True
                ).strip()
            except subprocess.CalledProcessError:
                self.skipTest("swiftc not available")
        src = self.getBuildArtifact("import_%s.swift" % module_name.lower())
        cache = self.getBuildArtifact(
            "swift-modules-cache-%s" % module_name.lower()
        )
        os.makedirs(cache, exist_ok=True)
        with open(src, "w") as f:
            f.write("import %s\n" % module_name)
        subprocess.check_call(
            [
                swiftc,
                "-typecheck",
                "-cxx-interoperability-mode=default",
                "-sdk",
                self._sdk_path(),
                "-module-cache-path",
                cache,
                "-F",
                framework_parent_dir,
                src,
            ]
        )

    def _require_framework(self):
        if not self.darwinWithFramework:
            self.skipTest("LLDB.framework not available")

    def _require_rpc(self):
        if not os.path.isfile(
            os.path.join(
                self._rpc_parked_headers_dir(), "Modules", "module.modulemap"
            )
        ):
            self.skipTest("LLDBRPC framework headers not built")

    def test_lldb_framework_objc_import(self):
        self._require_framework()
        self._compile_objcxx_import("LLDB", self.framework_dir)

    def test_lldbrpc_framework_objc_import(self):
        self._require_rpc()
        fw = self._stage_rpc_framework(self.getBuildDir())
        self._compile_objcxx_import("LLDBRPC", os.path.dirname(fw))

    def test_lldb_framework_swift_import(self):
        self._require_framework()
        self._compile_swift_import("LLDB", self.framework_dir)

    def test_lldbrpc_framework_swift_import(self):
        self._require_rpc()
        fw = self._stage_rpc_framework(self.getBuildDir())
        self._compile_swift_import("LLDBRPC", os.path.dirname(fw))
