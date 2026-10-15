"""Run large, freshly linked tools once before the API and Shell tests start.

On macOS, the first exec of a newly linked binary makes syspolicyd assess it
(Gatekeeper). For large tools like lldb-test and lldb-server the assessment
takes 30-50 seconds, and syspolicyd assesses nothing else while it runs. A test
inferior launched in that window is held in the kernel until it is assessed,
debugserver gives up on it after 30 seconds, and the test fails with "this is
a non-interactive debug session" or "process exited with status -1".

Running the tools once while lit is still loading its configuration, before
any test has launched a process, gets their assessment out of the way.
"""

import os
import platform
import subprocess
import time

# Each tool with arguments that make it exit right away. Only the exec matters.
TOOLS = [
    ("lldb-test", ["--version"]),
    ("lldb-server", ["version"]),
]

# Well above the longest assessment seen on CI, so that a slow assessment is
# waited out rather than cut short and repeated by the first test to use it.
TIMEOUT_SECONDS = 300

_done = False


def prewarm(lit_config, tools_dir):
    """Exec each of TOOLS found in tools_dir once per lit invocation."""
    global _done
    if _done or platform.system() != "Darwin" or not tools_dir:
        return
    _done = True

    for tool, args in TOOLS:
        path = os.path.join(tools_dir, tool)
        if not os.path.isfile(path):
            continue
        start = time.monotonic()
        try:
            subprocess.run(
                [path] + args,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=TIMEOUT_SECONDS,
            )
        except (OSError, subprocess.TimeoutExpired) as e:
            lit_config.warning("failed to prewarm {}: {}".format(path, e))
            continue
        lit_config.note(
            "prewarmed {} in {:.1f}s".format(tool, time.monotonic() - start)
        )
