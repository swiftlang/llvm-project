"""
Test end-to-end debugging of WebAssembly programs running in WasmKit.

Requires environment variables:
  WASMKIT          - path to the WasmKit binary
  WASI_SYSROOT     - path to the WASI sysroot (containing lib/libc.a etc.)
  WASI_RESOURCE_DIR - (optional) clang resource dir for wasm32-wasip1

Optional:
  CLANG            - path to clang with wasm32-wasip1 target support
                     (falls back to LLVM_TOOLS_DIR/clang)
"""

import lldb
import os
import shutil
import stat
import subprocess
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
from lldbsuite.test import lldbutil


def skipUnlessWasmKitIsInstalled(func):
    """Decorator that skips the test if WasmKit is not available."""

    def wrapper(self, *args, **kwargs):
        self._find_wasmkit()
        return func(self, *args, **kwargs)

    wrapper.__name__ = func.__name__
    wrapper.__doc__ = func.__doc__
    return wrapper


class TestWasmE2E(TestBase):
    NO_DEBUG_INFO_TESTCASE = True

    def _find_wasmkit(self):
        """Find the WasmKit binary or skip."""
        path = os.environ.get("WASMKIT")
        if path and os.path.isfile(path):
            self._wasmkit = path
            return
        found = shutil.which("wasmkit")
        if found:
            self._wasmkit = found
            return
        self.skipTest("WasmKit not found (set WASMKIT env var)")

    def _compile_wasm(self):
        """Compile Inputs/simple.c to a .wasm binary with DWARF."""
        clang = os.environ.get("CLANG")
        if not clang:
            tools_dir = os.environ.get("LLVM_TOOLS_DIR", "")
            candidate = os.path.join(tools_dir, "clang")
            if os.path.isfile(candidate):
                clang = candidate
        if not clang or not os.path.isfile(clang):
            self.skipTest("clang not found (set CLANG or LLVM_TOOLS_DIR env var)")
        sysroot = os.environ.get("WASI_SYSROOT")
        if not sysroot or not os.path.isdir(sysroot):
            self.skipTest("WASI sysroot not found (set WASI_SYSROOT env var)")
        resource_dir = os.environ.get("WASI_RESOURCE_DIR", "")

        src = self.getSourcePath(os.path.join("Inputs", "simple.c"))
        out = self.getBuildArtifact("simple.wasm")
        cmd = [
            clang,
            "--target=wasm32-wasip1",
            f"--sysroot={sysroot}",
            "-g", "-O0",
            "-o", out,
            src,
        ]
        if resource_dir and os.path.isdir(resource_dir):
            cmd.insert(3, f"-resource-dir={resource_dir}")
        subprocess.check_call(cmd)
        return out

    def _setup_platform(self):
        """Configure PlatformWasm settings for WasmKit.

        PlatformWasm builds the command line as:
            [runtime-path, port-arg+port, runtime-args..., wasm-file]
        WasmKit's --debugger-port is an option of the 'run' subcommand,
        so it must come after 'run':
            wasmkit run --debugger-port=PORT file.wasm
        Since PlatformWasm always places port-arg before runtime-args,
        we use a wrapper script as runtime-path that inserts 'run'.
        """
        wrapper = self.getBuildArtifact("wasmkit-wrapper.sh")
        with open(wrapper, "w") as f:
            f.write("#!/bin/sh\n")
            f.write(f'exec "{self._wasmkit}" run "$@"\n')
        os.chmod(wrapper, os.stat(wrapper).st_mode | stat.S_IEXEC)

        self.runCmd("settings set platform.plugin.wasm.runtime-path " + wrapper)
        # Use '=' so PlatformWasm can concatenate port directly:
        # --debugger-port=PORT (no space needed).
        # The '--' before the variable name prevents LLDB from parsing the
        # value as an option to 'settings set' itself.
        self.runCmd(
            "settings set -- platform.plugin.wasm.port-arg --debugger-port=")

    def _launch(self, wasm_file):
        """Create target and launch via PlatformWasm::DebugProcess."""
        target = self.dbg.CreateTarget(wasm_file)
        self.assertTrue(target.IsValid(), "Failed to create target")

        launch_info = lldb.SBLaunchInfo(None)
        launch_info.SetLaunchFlags(lldb.eLaunchFlagStopAtEntry)
        error = lldb.SBError()
        process = target.Launch(launch_info, error)
        self.assertSuccess(error, "Failed to launch via PlatformWasm")
        self.assertTrue(process.IsValid())

        return target, process

    @skipIfRemote
    @skipIfAsan
    @skipUnlessWasmKitIsInstalled
    def test_debug_process(self):
        """Test setting a breakpoint and inspecting the call stack."""
        wasm_file = self._compile_wasm()
        self._setup_platform()
        target, process = self._launch(wasm_file)

        # Set breakpoint on 'add' after module is loaded.
        bp = target.BreakpointCreateByName("add")
        self.assertTrue(bp.IsValid())

        # Continue to hit the breakpoint.
        process.Continue()
        self.assertEqual(process.GetState(), lldb.eStateStopped)

        thread = process.GetThreadAtIndex(0)
        self.assertTrue(thread.IsValid())

        frame = thread.GetFrameAtIndex(0)
        self.assertTrue(frame.IsValid())
        self.assertIn("add", frame.GetFunctionName())

        # Verify arguments: add(1, 2).
        a = frame.FindVariable("a")
        self.assertTrue(a.IsValid())
        self.assertEqual(a.GetValueAsSigned(), 1)
        b = frame.FindVariable("b")
        self.assertTrue(b.IsValid())
        self.assertEqual(b.GetValueAsSigned(), 2)

        # Call stack: add() called from main() (or _start).
        self.assertGreaterEqual(thread.GetNumFrames(), 2)

        process.Kill()
