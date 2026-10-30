import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil

import ctypes
import gc
import os
import sys
import time


class ReplaceDllTestCase(TestBase):
    SHARED_BUILD_TESTCASE = False

    # ---------------------------------------------------------------- DIAGNOSTIC
    # Everything below exists to explain a `PermissionError: [WinError 5]` on
    # os.remove(foo) from a CI console log alone. Remove once understood.

    def _diag(self, *args):
        print("###", *args, flush=True)

    def _diag_cmd(self, cmd):
        res = lldb.SBCommandReturnObject()
        self.dbg.GetCommandInterpreter().HandleCommand(cmd, res)
        self._diag(f"--- {cmd} ---")
        print(res.GetOutput() or res.GetError() or "<no output>", flush=True)

    def _diag_try_open(self, path):
        """Report whether the file can be opened for delete, and with what sharing."""
        GENERIC_READ, GENERIC_WRITE, DELETE = 0x80000000, 0x40000000, 0x00010000
        OPEN_EXISTING, INVALID = 3, ctypes.c_void_p(-1).value
        CreateFileW = ctypes.windll.kernel32.CreateFileW
        CreateFileW.restype = ctypes.c_void_p
        for label, access, share in (
            ("read, shared", GENERIC_READ, 0x1 | 0x2 | 0x4),
            ("read, exclusive", GENERIC_READ, 0),
            ("delete-access", DELETE, 0x1 | 0x2 | 0x4),
            ("write, exclusive", GENERIC_READ | GENERIC_WRITE, 0),
        ):
            h = CreateFileW(
                ctypes.c_wchar_p(path), access, share, None, OPEN_EXISTING, 0, None
            )
            if h == INVALID:
                err = ctypes.GetLastError()
                self._diag(
                    f"open({label}) -> FAILED err={err} ({ctypes.FormatError(err)})"
                )
            else:
                ctypes.windll.kernel32.CloseHandle(ctypes.c_void_p(h))
                self._diag(f"open({label}) -> OK")

    def _diag_processes(self):
        """Any inferior still alive is a candidate holder."""
        import subprocess
        try:
            out = subprocess.run(
                ["tasklist", "/fo", "csv", "/nh"],
                capture_output=True, text=True, timeout=60,
            ).stdout
        except Exception as e:
            self._diag(f"tasklist failed: {e!r}")
            return
        interesting = [
            l for l in out.splitlines()
            if any(n in l.lower() for n in ("a.out", "lldb", "python", "conhost"))
        ]
        self._diag(f"processes of interest ({len(interesting)}):")
        for l in interesting:
            print("   ", l, flush=True)

    def _diag_referrers(self, obj, label):
        self._diag(f"{label}: refcount={sys.getrefcount(obj)}")
        for i, r in enumerate(gc.get_referrers(obj)):
            desc = type(r).__name__
            if isinstance(r, dict):
                keys = [k for k in r if not str(k).startswith("__")][:8]
                desc += f" keys={keys}"
            elif isinstance(r, (list, tuple, set)):
                desc += f" len={len(r)}"
            self._diag(f"  referrer[{i}] {desc}")

    def _diag_remove_with_retry(self, path, timeout_s=30.0):
        """Retry the delete, timestamped.

        Whether it ever succeeds is the key datum: a delete that clears after a
        delay means a mapping draining after process exit, one that never clears
        means something still holds a reference.
        """
        start = time.monotonic()
        attempt = 0
        last = None
        while time.monotonic() - start < timeout_s:
            attempt += 1
            try:
                os.remove(path)
                self._diag(
                    f"os.remove SUCCEEDED on attempt {attempt} after "
                    f"{time.monotonic() - start:.2f}s"
                )
                return True
            except OSError as e:
                if last is None or str(e) != str(last):
                    self._diag(
                        f"os.remove attempt {attempt} at "
                        f"{time.monotonic() - start:.2f}s -> {e!r}"
                    )
                    last = e
            time.sleep(0.1)
        self._diag(
            f"os.remove NEVER succeeded in {timeout_s}s over {attempt} attempts"
        )
        return False

    def _dump_lldb_logs(self, max_lines=600):
        """Print the lldb logs to stdout; CI gives us nothing but the console."""
        # Logging is buffered, and only closing the stream flushes it, so the
        # channels have to come down before the files can be read back.
        self.runCmd("log disable lldb windows", check=False)
        for channel, path in getattr(self, "_lldb_logs", {}).items():
            if not os.path.isfile(path):
                self._diag(f"{channel}: no log at {path}")
                continue
            with open(path, "r", errors="replace") as f:
                lines = f.read().splitlines()
            self._diag(
                f"--- {channel} log: {len(lines)} lines, "
                f"{os.path.getsize(path)} bytes ---"
            )
            keep = [
                l
                for l in lines
                if any(
                    k in l
                    for k in (
                        "foo",
                        "Module",
                        "module",
                        "ObjectFile",
                        "orphan",
                        "Unload",
                        "Load",
                        "DLL",
                        "dll",
                    )
                )
            ]
            self._diag(f"{len(keep)} matching lines; showing the last {max_lines}")
            for l in keep[-max_lines:]:
                print("   ", l, flush=True)
            self._diag(f"--- last 40 lines of {channel} verbatim ---")
            for l in lines[-40:]:
                print("   ", l, flush=True)

    @requireWindows
    def test(self):
        """
        Test that LLDB unlocks module files once all references are released.
        """

        exe = self.getBuildArtifact("a.out")
        foo = self.getBuildArtifact("foo.dll")
        bar = self.getBuildArtifact("bar.dll")

        self._lldb_logs = {
            "lldb": self.getBuildArtifact("diag-lldb.log"),
            "windows": self.getBuildArtifact("diag-windows.log"),
        }
        self.runCmd(
            "log enable -f %s lldb module object target platform process"
            % self._lldb_logs["lldb"]
        )
        self.runCmd(
            "log enable -f %s windows process event" % self._lldb_logs["windows"]
        )
        self.addTearDownHook(lambda: self.runCmd("log disable lldb windows"))

        self.build(
            dictionary={
                "DYLIB_NAME": "foo",
                "DYLIB_C_SOURCES": "foo.c",
                "C_SOURCES": "test.c",
            }
        )
        self.build(
            dictionary={
                "DYLIB_ONLY": "YES",
                "DYLIB_NAME": "bar",
                "DYLIB_C_SOURCES": "bar.c",
            }
        )

        target = self.dbg.CreateTarget(exe)
        self.assertTrue(target, VALID_TARGET)

        shlib_names = ["foo"]
        environment = self.registerSharedLibrariesWithTarget(target, shlib_names)
        process = target.LaunchSimple(
            None, environment, self.get_process_working_directory()
        )
        self.assertEqual(process.GetExitStatus(), 42)

        module = next((m for m in target.modules if "foo" in m.file.basename), None)
        self.assertIsNotNone(module)
        # lldb reports the canonical path, which differs from the build
        # artifact path when the build tree is reached through a subst drive.
        self.assertEqual(os.path.realpath(module.file.fullpath), os.path.realpath(foo))

        self._diag("=== before RemoveModule ===")
        self._diag_cmd("target modules list")
        self._diag_cmd("statistics dump")
        self._diag_referrers(module, "module before RemoveModule")

        target.RemoveModule(module)
        self._diag("=== after RemoveModule ===")
        self._diag_referrers(module, "module after RemoveModule")
        del module
        gc.collect()

        self.dbg.MemoryPressureDetected()

        self._diag("=== after MemoryPressureDetected ===")
        self._diag_cmd("target modules list")
        self._diag_cmd("statistics dump")
        self._diag(f"process state={process.GetState()} "
                   f"exit={process.GetExitStatus()} "
                   f"num_targets={self.dbg.GetNumTargets()} "
                   f"num_modules={target.GetNumModules()}")
        self._diag_processes()
        self._diag_try_open(foo)

        if not self._diag_remove_with_retry(foo):
            self._diag("=== state after the delete window expired ===")
            self._diag_try_open(foo)
            self._diag_processes()
            self._diag_cmd("target modules list")
            self._dump_lldb_logs()
            os.remove(foo)  # re-raise the original PermissionError

        os.rename(bar, foo)

        process = target.LaunchSimple(
            None, environment, self.get_process_working_directory()
        )
        self.assertEqual(process.GetExitStatus(), 43)
