"""Shared utilities for bounds-safety test scripts."""

import logging
import sys

log = logging.getLogger(__name__)


class LLDBLaunchError(Exception):
    """Raised when LLDB target creation or process launch fails."""
    pass


def import_lldb(lldb_python_path):
    """Import and return the lldb module, adjusting sys.path if needed."""
    if lldb_python_path:
        sys.path.insert(0, lldb_python_path)
    import lldb
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

    def __exit__(self, exc_type, exc_val, exc_tb):
        if self.process is not None:
            try:
                if self.process.is_alive:
                    self.process.Kill()
            except Exception:
                pass
        return False
