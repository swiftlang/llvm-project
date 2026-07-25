import os
import subprocess
import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


def _spawn_hang_watchdog(secs):
    # rdar://182785830: this test intermittently HANGS (only under CI load) in the
    # noclang reflection resolution of `c.v` -- a stored property reached through a
    # cross-module resilient *generic* superclass rooted at NSObject. lit just
    # SIGKILLs it at the 600s timeout (Exit Code -9) with no backtrace, so we never
    # see where it is stuck. This independent child process (GIL-independent, so it
    # works even if the hang holds the Python GIL) waits `secs` and then samples the
    # possibly-stuck lldb/dotest process, dumping the C++ backtrace to stderr so the
    # failing CI run self-documents. Remove before merging.
    pid = os.getpid()
    out = "/tmp/rdar182785830-hang-%d.txt" % pid
    cmd = ("sleep %d; "
           "echo '*** rdar182785830 WATCHDOG: c.v unresolved after %ds; sampling pid %d ***' 1>&2; "
           "/usr/bin/sample %d 10 1 2>&1 | tee %s 1>&2"
           % (secs, secs, pid, pid, out))
    return subprocess.Popen(["/bin/sh", "-c", cmd])


class TestSwiftResilienceSuperclassOtherMod(TestBase):
    @skipEmbeddedSwift
    @skipUnlessDarwin
    @swiftTest
    def test(self):
        self.build()
        target, process, thread, bkpt = lldbutil.run_to_source_breakpoint(
            self, 'break here', lldb.SBFileSpec('ModWithClass.swift'),
            extra_images=['ModWithClass', 'ModWithSuper'])

        # rdar://182785830 hang instrumentation (see _spawn_hang_watchdog above).
        secs = int(os.environ.get("RDAR182785830_WATCHDOG_SECS", "120"))
        wd = _spawn_hang_watchdog(secs)
        try:
            self.expect("expression c.v", substrs=["Int", "42"])
        finally:
            wd.terminate()
