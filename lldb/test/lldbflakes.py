"""Rerun policy shared by the API and Shell test formats."""

import platform
import re

import lit.Test

# Failures that come from flaky infrastructure rather than from the test
# itself. Rerunning masks real bugs, so this is deliberately not a blanket
# retry of every failure. Adding an entry is a last resort, reserved for a
# defect outside LLDB's control that cannot be worked around any other way.
KNOWN_FLAKES = [
    # macOS 26.3 denies debugserver permission to attach when too many debug
    # sessions start at once, which surfaces as an immediate process exit.
    re.compile(r"process exited with status -1"),
]

MAX_ATTEMPTS = 3

# On Windows, any failure or timeout is rerun until the flakes in LLDB's
# Windows support are fixed.
# rdar://188906589
RERUN_ALL_FAILURES = platform.system() == "Windows"


def _hit_known_flake(output):
    return any(flake.search(output) for flake in KNOWN_FLAKES)


def _should_rerun(result):
    if RERUN_ALL_FAILURES:
        return result.code in (lit.Test.FAIL, lit.Test.UNRESOLVED, lit.Test.TIMEOUT)
    return result.code == lit.Test.FAIL and _hit_known_flake(result.output)


def execute_with_reruns(execute_once):
    """Run execute_once, which returns a lit.Test.Result, until it stops
    failing with a known flake, or with any failure on Windows."""
    outputs = []
    for attempt in range(MAX_ATTEMPTS):
        result = execute_once()
        outputs.append(
            "Attempt %d of %d: %s\n%s"
            % (attempt + 1, MAX_ATTEMPTS, result.code.name, result.output)
        )
        if not _should_rerun(result):
            break

    # A pass that needed a rerun is reported apart from a clean pass so that
    # the flakiness stays visible.
    if attempt > 0:
        if result.code == lit.Test.PASS:
            result.code = lit.Test.FLAKYPASS
        result.attempts = attempt + 1
        result.max_allowed_attempts = MAX_ATTEMPTS
        result.output = "\n".join(outputs)

    return result
