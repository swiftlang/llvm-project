"""Shared utilities for bounds-safety test scripts."""

import logging
import os
import sys

log = logging.getLogger(__name__)


class LLDBLaunchError(Exception):
    """Raised when LLDB target creation or process launch fails."""
    pass


def import_lldb(lldb_python_path):
    """Import and return the lldb module, adjusting sys.path if needed.
    """
    if lldb_python_path:
        sys.path.insert(0, lldb_python_path)
    import lldb
    # The interpreter running these scripts may already have an unrelated `lldb`
    # module installed (e.g. one shipped in the toolchain's site-packages for a
    # different LLDB). Inserting lldb_python_path at the front of sys.path makes
    # our copy win, but to be sure we didn't silently pick up a different one we
    # verify the imported module actually lives under lldb_python_path.
    if lldb_python_path:
        expected = os.path.realpath(lldb_python_path)
        actual = os.path.realpath(lldb.__file__)
        if os.path.commonpath([expected, actual]) != expected:
            raise LLDBLaunchError(
                f"imported the wrong lldb module: expected it under '{expected}' but "
                f"loaded '{lldb.__file__}'. Another lldb may be installed earlier on "
                "sys.path."
            )
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
        """Kill the process (if any) and tear LLDB down cleanly.

        The Terminate() call is important: without it, an assertions-enabled
        LLDB (e.g. a just-built in-tree lldb) aborts at interpreter shutdown
        with "forgot to unregister plugin?", turning a passing test into a
        spurious failure.
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
            try:
                # Needed to avoid an assertion in LLDB:
                #
                # Assertion failed: (m_instances.empty() && "forgot to unregister plugin?").
                #
                # FIXME: Technically this shouldn't be here because it prevents
                # using this class multiple times in a script but right now this
                # is the most convenient place to put this.
                self._lldb.SBDebugger.Terminate()
            except Exception:
                pass

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._cleanup()
        return False

