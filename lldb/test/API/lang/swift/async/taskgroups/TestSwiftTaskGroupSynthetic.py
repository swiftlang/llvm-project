import textwrap
import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil


class TestCase(TestBase):

    @requireNotEmbeddedSwift
    @swiftTest
    @expectedFailureAll(debug_info=["pdb"])
    def test(self):
        """Verify the children of a TaskGroup and a ThrowingTaskGroup."""
        self.build()
        src = lldb.SBFileSpec("main.swift")
        target, process, thread, bkpt = lldbutil.run_to_source_breakpoint(
            self, "break here TaskGroup", src
        )
        target.BreakpointDelete(bkpt.GetID())
        self.do_test_api(thread)
        self.do_test_print()

        # Awaiting the first group may resume on a different thread.
        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break here ThrowingTaskGroup", src
        )
        self.assertEqual(len(threads), 1)
        self.do_test_api(threads[0])
        self.do_test_print()

    def do_test_print(self):
        self.expect(
            "v group",
            patterns=[
                textwrap.dedent(
                    r"""
                    \((?:Throwing)?TaskGroup<\(\)\??(?:, Error)?>\) group = \{
                      \[0\] = id:([1-9]\d*) flags:(?:suspended\|)?(?:running\|)?(?:enqueued\|)?groupChildTask \{
                        address = 0x[0-9a-f]+
                        id = \1
                        enqueuePriority = \.medium
                        parent = (.+)
                        children = \{\}
                      \}
                      \[1\] = id:([1-9]\d*) flags:(?:suspended\|)?(?:running\|)?(?:enqueued\|)?groupChildTask \{
                        address = 0x[0-9a-f]+
                        id = \3
                        enqueuePriority = \.medium
                        parent = \2
                        children = \{\}
                      \}
                      \[2\] = id:([1-9]\d*) flags:(?:suspended\|)?(?:running\|)?(?:enqueued\|)?groupChildTask \{
                        address = 0x[0-9a-f]+
                        id = \4
                        enqueuePriority = \.medium
                        parent = \2
                        children = \{\}
                      \}
                    \}
                    """
                ).strip()
            ],
        )

    def do_test_api(self, thread):
        frame = thread.GetSelectedFrame()
        group = frame.FindVariable("group")
        self.assertEqual(group.num_children, 3)
        for task in group:
            self.assertEqual(str(task), str(group.GetChildMemberWithName(task.name)))
            self.assertEqual(
                task.GetChildMemberWithName("isGroupChildTask").summary, "true"
            )
