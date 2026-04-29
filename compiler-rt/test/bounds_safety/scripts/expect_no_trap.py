#!/usr/bin/env python3
"""Verify a binary runs without trapping.

It supports running in two modes. In both modes this succeeds if the following
are true:

* The binary exits with a zero exit code.
* In soft trap mode a soft trap is not hit.

The binary can be run in one of two modes. The distinction between running in
these two modes only matters when the binary fails.

Direct mode: Run binary directly. If the binary fails the only available 
             information is the exit code.
Debugger mode: Run binary under debugger. A backtrace is printed where the
               binary unexpectedly stopped.
"""

import argparse
import logging
import os
import subprocess
import sys

log = logging.getLogger("expect_no_trap")


SOFT_TRAP_MARKER = "***HIT BOUNDS SAFETY SOFT TRAP***:"


def run_direct(binary, args, soft_traps=False):
    if soft_traps:
        result = subprocess.run([binary] + args, capture_output=True, text=True)
        if result.returncode != 0:
            if result.returncode < 0:
                log.error("process was killed by signal %d", -result.returncode)
            else:
                log.error("process exited with status %d", result.returncode)
            return 1
        if SOFT_TRAP_MARKER in result.stderr:
            log.error("unexpected soft trap detected in stderr")
            log.error("stderr: %s", result.stderr)
            return 1
        return 0

    result = subprocess.run([binary] + args)
    if result.returncode == 0:
        return 0
    if result.returncode < 0:
        log.error("process was killed by signal %d", -result.returncode)
    else:
        log.error("process exited with status %d", result.returncode)
    return 1


def check_no_trap(lldb, process):
    """Verify the process exited cleanly without trapping. Returns 0 on success."""
    state = process.GetState()

    if state == lldb.eStateExited:
        exit_status = process.GetExitStatus()
        if exit_status == 0:
            return 0
        log.error("process exited with status %d", exit_status)
        return 1

    if state == lldb.eStateStopped:
        thread = process.GetSelectedThread()
        stop_reason = thread.GetStopDescription(256)
        log.error("process stopped unexpectedly: %s", stop_reason)
        log.info("Backtrace:")
        for frame in thread:
            log.info("  %s", frame)
        return 1

    log.error("unexpected process state: %s", state)
    return 1


def run_debugger(binary, args, lldb_python_path):
    from lldb_utils import LLDBLaunchError, LLDBProcessContextManager, import_lldb

    lldb = import_lldb(lldb_python_path)

    with LLDBProcessContextManager(lldb, binary, args) as ctx:
        try:
            ctx.launch()
        except LLDBLaunchError as e:
            log.error("%s", e)
            return 1
        return check_no_trap(lldb, ctx.process)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--log-level", default="info",
                        choices=["debug", "info", "warning", "error"],
                        help="Logging level (default: warning)")
    parser.add_argument("--use-debugger", action="store_true")
    parser.add_argument("--lldb-python-path", default=None)
    parser.add_argument("--soft-traps", action="store_true",
                        help="Expect soft traps (non-fatal)")
    parser.add_argument("binary")
    parser.add_argument("args", nargs="*")

    parsed = parser.parse_args()

    logging.basicConfig(
        level=getattr(logging, parsed.log_level.upper()),
        format="%(name)s: %(levelname)s: %(message)s",
    )

    if not os.path.isfile(parsed.binary):
        log.error("binary not found: '%s'", parsed.binary)
        return 1

    if parsed.use_debugger:
        return run_debugger(parsed.binary, parsed.args, parsed.lldb_python_path)
    return run_direct(parsed.binary, parsed.args, parsed.soft_traps)


if __name__ == "__main__":
    sys.exit(main())
