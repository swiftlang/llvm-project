"""Run freshly linked tools once before the API and Shell tests start.

On macOS, the first exec of a newly linked binary makes syspolicyd assess it
(Gatekeeper). For large tools like lldb-test and lldb-server the assessment
takes 30-50 seconds, and syspolicyd assesses nothing else while it runs. A test
inferior launched in that window is held in the kernel until it is assessed,
debugserver gives up on it after 30 seconds, and the test fails with "this is
a non-interactive debug session" or "process exited with status -1".

Running the tools once while lit is still loading its configuration, before
any test has launched a process, gets their assessment out of the way. This
covers every binary in the build that the tests launch, directly or through
another tool (lldb launching debugserver, clang running the linker). The test
inferiors themselves are built by the tests and can't be warmed up here. The
unit test binaries don't need it either, because lit runs each of them with
--gtest_list_tests while it discovers the tests.
"""

import os
import platform
import subprocess
import time

# Each tool with arguments that make it exit right away. Only the exec matters.
# Tools that are links to the same file (clang++ and clang-cl to clang, swiftc
# to swift-frontend, llvm-strip to llvm-objcopy) are only run once.
TOOLS = [
    # LLDB and the tools it launches.
    ("lldb", ["--version"]),
    ("lldb-test", ["--version"]),
    ("lldb-server", ["version"]),
    ("debugserver", ["--version"]),
    ("lldb-dap", ["--version"]),
    ("lldb-mcp", ["--version"]),
    ("lldb-rpc-gen", ["--version"]),
    ("lldb-argdumper", []),
    ("darwin-debug", []),
    ("darwin-mte-launcher", ["/usr/bin/true"]),
    ("repl_swift", []),
    ("yaml2macho-core", ["--version"]),
    # Compilers and linkers that build the test inferiors.
    ("clang", ["--version"]),
    ("clang-scan-deps", ["--version"]),
    ("ld64.lld", ["--version"]),
    ("ld.lld", ["--version"]),
    ("dsymutil", ["--version"]),
    ("swiftc", ["--version"]),
    ("swift-frontend", ["--version"]),
    ("swift-plugin-server", []),
    # LLVM tools used in RUN lines and by the API test Makefiles.
    ("FileCheck", ["--version"]),
    ("count", []),
    ("not", []),
    ("split-file", ["--version"]),
    ("yaml2obj", ["--version"]),
    ("obj2yaml", ["--version"]),
    ("llc", ["--version"]),
    ("lli", ["--version"]),
    ("llvm-ar", ["--version"]),
    ("llvm-dwarfdump", ["--version"]),
    ("llvm-dwp", ["--version"]),
    ("llvm-mc", ["--version"]),
    ("llvm-nm", ["--version"]),
    ("llvm-objcopy", ["--version"]),
    ("llvm-objdump", ["--version"]),
    ("llvm-pdbutil", ["--version"]),
    ("llvm-readobj", ["--version"]),
]

# Well above the longest assessment seen on CI, so that a slow assessment is
# waited out rather than cut short and repeated by the first test to use it.
TIMEOUT_SECONDS = 300

# Real paths of the binaries that were already run by this lit invocation. The
# API and Shell suites share most of their tools.
_warmed = set()


def _tool_dirs(config):
    """Return the directories the tests take the tools in TOOLS from."""
    dirs = []

    def add_dir(attr, subdir=None):
        path = getattr(config, attr, None)
        if path:
            dirs.append(os.path.join(path, subdir) if subdir else path)

    def add_dir_of(attr):
        path = getattr(config, attr, None)
        if path:
            dirs.append(os.path.dirname(path))

    add_dir("lldb_tools_dir")
    add_dir_of("lldb_executable")
    # In a framework build, debugserver, lldb-argdumper and darwin-debug live
    # in the framework.
    add_dir("lldb_framework_dir", "Resources")
    add_dir("lldb_lit_tools_dir")
    add_dir("llvm_tools_dir")
    add_dir_of("test_compiler")
    add_dir_of("test_swift_compiler")
    add_dir_of("swiftc")
    add_dir_of("dsymutil")
    # The shims in /usr/bin and the tools they run are signed by Apple.
    return [d for d in dirs if not os.path.realpath(d).startswith("/usr/")]


def prewarm(lit_config, config):
    """Exec each of TOOLS found in the tool directories of config once."""
    if platform.system() != "Darwin":
        return

    dirs = _tool_dirs(config)
    for tool, args in TOOLS:
        for tools_dir in dirs:
            path = os.path.join(tools_dir, tool)
            if not os.path.isfile(path) or not os.access(path, os.X_OK):
                continue
            real_path = os.path.realpath(path)
            if real_path in _warmed:
                continue
            _warmed.add(real_path)
            start = time.monotonic()
            try:
                subprocess.run(
                    [path] + args,
                    stdin=subprocess.DEVNULL,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                    timeout=TIMEOUT_SECONDS,
                )
            except (OSError, subprocess.TimeoutExpired) as e:
                lit_config.warning("failed to prewarm {}: {}".format(path, e))
                continue
            lit_config.note(
                "prewarmed {} in {:.1f}s".format(path, time.monotonic() - start)
            )
