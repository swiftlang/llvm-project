"""Shared utilities for bounds-safety test scripts."""

import atexit
import logging
import os
import sys
import threading

log = logging.getLogger(__name__)
# Used to serialize `import_lldb`
_import_lock = threading.Lock()
_terminate_registered = False


class LLDBLaunchError(Exception):
    """Raised when LLDB target creation or process launch fails."""
    pass


def import_lldb(lldb_python_path):
    """Import and return the lldb module, adjusting sys.path if needed.

    Thread-safe: concurrent calls are serialized so the shared sys.path
    mutation and the one-time process teardown registration cannot race.
    """
    global _terminate_registered
    with _import_lock:
        if lldb_python_path:
            sys.path.insert(0, lldb_python_path)
        import lldb
        # The interpreter running these scripts may already have an unrelated
        # `lldb` module installed (e.g. one shipped in the toolchain's
        # site-packages for a different LLDB). Inserting lldb_python_path at the
        # front of sys.path makes our copy win, but to be sure we didn't
        # silently pick up a different one we verify the imported module
        # actually lives under lldb_python_path.
        if lldb_python_path:
            expected = os.path.realpath(lldb_python_path)
            actual = os.path.realpath(lldb.__file__)
            if os.path.commonpath([expected, actual]) != expected:
                raise LLDBLaunchError(
                    f"imported the wrong lldb module: expected it under '{expected}' but "
                    f"loaded '{lldb.__file__}'. Another lldb may be installed earlier on "
                    "sys.path."
                )

        if not _terminate_registered:
            # SBDebugger.Terminate() is the process-global counterpart to the
            # SBDebugger.Initialize() that the lldb Python module runs at import
            # time (lldb.py runs `SBDebugger.Initialize()` at module scope),
            # which registers all plugins. It must run once, at process exit,
            # after all debuggers are destroyed; otherwise an assertions-enabled
            # LLDB aborts at shutdown with:
            #
            #   Assertion failed: (m_instances.empty() && "forgot to unregister
            #   plugin?").
            #
            # atexit runs before the LLDB library's static plugin-instance
            # destructors, so registering it here is sufficient.
            atexit.register(lldb.SBDebugger.Terminate)
            _terminate_registered = True

    return lldb


class LLDBProcessContextManager:
    """Context manager that creates an LLDB target and kills the process on exit.

    Usage:
        lldb = import_lldb(lldb_python_path)
        with LLDBProcessContextManager(lldb, binary, args) as ctx:
            # ctx.debugger and ctx.target are available here
            # Set breakpoints, check plugins, etc. before launching
            ctx.launch()  # raises LLDBLaunchError on failure
            # ctx.process is the SBProcess
    """

    def __init__(self, lldb, binary, args):
        self._lldb = lldb
        self._binary = binary
        self._args = args
        self.debugger = None
        self.target = None
        self.process = None

    def __enter__(self):
        self.debugger = self._lldb.SBDebugger.Create()
        self.debugger.SetAsync(False)

        self.target = self.debugger.CreateTarget(self._binary)
        if not self.target:
            # Tear down before raising: __exit__ is not called when __enter__
            # raises, so without this the debugger created above would leak and
            # an assertions-enabled LLDB would abort at interpreter shutdown.
            self._cleanup()
            raise LLDBLaunchError(
                "could not create target for '{}'".format(self._binary)
            )

        return self

    def launch(self):
        """Launch the process. Raises LLDBLaunchError on failure."""
        launch_info = self._lldb.SBLaunchInfo(self._args)
        launch_info.SetLaunchFlags(0)

        error = self._lldb.SBError()
        self.process = self.target.Launch(launch_info, error)
        if not self.process or not error.Success():
            self.process = None
            raise LLDBLaunchError(
                "could not launch process: {}".format(error)
            )

    def _cleanup(self):
        """Kill the process (if any) and destroy this instance's debugger.

        The process-global SBDebugger.Terminate() teardown is handled separately
        via an atexit handler registered in import_lldb(); it must not run here
        because it is a once-per-process operation.
        """
        if self.process is not None:
            try:
                if self.process.is_alive:
                    self.process.Kill()
            except Exception:
                pass
            self.process = None
        if self.debugger is not None:
            try:
                self._lldb.SBDebugger.Destroy(self.debugger)
            except Exception:
                pass
            self.debugger = None

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._cleanup()
        return False

