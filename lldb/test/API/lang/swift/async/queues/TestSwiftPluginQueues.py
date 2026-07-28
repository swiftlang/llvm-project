import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil
import re


class TestCase(lldbtest.TestBase):
    def run_to_task_thread(self):
        """Stops in an async function and returns the selected Task thread."""
        self.build()

        self.runCmd("settings set target.experimental.swift-tasks-plugin-enabled true")

        source_file = lldb.SBFileSpec("main.swift")
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "BREAK HERE", source_file
        )
        return thread

    @skipEmbeddedSwift
    @swiftTest
    # rdar://183113449: on Windows the concurrency runtime keeps the current
    # task in a C++ thread_local, but it reports layout version 2, for which
    # DeriveStorageKind assumes Darwin's pthread_reserved_key scheme. lldb
    # therefore picks a task finder that cannot see the task and no Task thread
    # is created. Teaching DeriveStorageKind to pick cxx_thread_local off
    # Darwin does make this pass, but it also activates the tasks plugin across
    # the whole suite, and ProcessWindows does not tolerate OS-plugin threads
    # (resuming crashes in Process::Continue). Both need fixing together.
    @skipIf(oslist=["windows", "linux"])
    def test_task_thread(self):
        """Test that the plugin presents the running task as a thread."""
        thread = self.run_to_task_thread()
        self.assertRegex(thread.GetName(), r"^Task [1-9]$")

    @skipEmbeddedSwift
    @swiftTest
    # Queue names come from libdispatch introspection, which only the Darwin
    # process plugins implement; ProcessWindows does not report a queue at all.
    @skipUnlessDarwin
    def test_queue_name(self):
        """Test that a Task thread reports its backing thread's queue."""
        thread = self.run_to_task_thread()

        queue_plugin = self.get_queue_from_thread_info_command(False)
        queue_backing = self.get_queue_from_thread_info_command(True)
        self.assertEqual(queue_plugin, queue_backing)
        self.assertEqual(queue_plugin, thread.GetQueueName())

    queue_regex = re.compile(r"queue = '([^']+)'")

    def get_queue_from_thread_info_command(self, use_backing_thread):
        interp = self.dbg.GetCommandInterpreter()
        result = lldb.SBCommandReturnObject()

        backing_thread_arg = ""
        if use_backing_thread:
            backing_thread_arg = "--backing-thread"

        interp.HandleCommand(
            "thread info {0}".format(backing_thread_arg),
            result,
            True,
        )
        self.assertTrue(result.Succeeded(), "failed to run thread info")
        match = self.queue_regex.search(result.GetOutput())
        self.assertNotEqual(match, None)
        return match.group(1)
