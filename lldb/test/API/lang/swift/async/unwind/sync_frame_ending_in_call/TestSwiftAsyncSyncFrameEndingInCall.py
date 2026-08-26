"""
Test that a sync frame whose last instruction is a call is not mistaken for an
async frame.

Such a frame's return address is one past the end of the function, i.e. the first
byte of whichever function was placed next.  When that neighbour is an async
funclet, resolving the return address without backing it up by one makes
SwiftLanguageRuntime::GetRuntimeUnwindPlan build an async unwind plan -- CFA
taken from the async context register -- for a plain sync frame.  The unwind then
stops dead at that frame and every caller above it is lost.

The binary is hand-written assembly using real Swift mangled names, which is all
the Swift language runtime inspects.  No Swift compiler is involved, so the
layout the test depends on is fixed by the source rather than chosen by an
optimizer.
"""

import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftAsyncSyncFrameEndingInCall(lldbtest.TestBase):

    mydir = lldbtest.TestBase.compute_mydir(__file__)

    NO_DEBUG_INFO_TESTCASE = True

    # swift-async-decoy.s is AArch64 assembly.
    @skipIf(archs=no_match(["aarch64", "arm64", "arm64e"]))
    @skipIf(oslist=no_match(lldbplatformutil.getDarwinOSTriples()))
    def test(self):
        """Test that no frames are lost above a sync frame ending in a call"""
        self.build()

        target = self.dbg.CreateTarget(self.getBuildArtifact("a.out"))
        self.assertTrue(target, lldbtest.VALID_TARGET)

        process = target.LaunchSimple(None, None, self.get_process_working_directory())
        self.assertTrue(process, lldbtest.PROCESS_IS_VALID)
        thread = lldbutil.get_stopped_thread(process, lldb.eStopReasonException)
        self.assertTrue(thread, "Failed to stop on the bad memory access")

        self.check_layout(target)

        expected = [
            "reportAndDie",
            "doPanic",
            "waitComplete",
            "caller",
            "enter_swift",
            "main",
        ]
        names = [frame.GetFunctionName() or "" for frame in thread.frames]
        self.assertGreaterEqual(
            len(names), len(expected), "backtrace was truncated: %s" % names
        )
        for index, expected_name in enumerate(expected):
            self.assertIn(expected_name, names[index], "frame #%d" % index)

    def check_layout(self, target):
        """The two properties the bug needs.  Both are fixed by
        swift-async-decoy.s, so a failure here means the assembler or linker
        reordered something rather than that a compiler changed its mind."""
        sync = self.find_symbol(target, "$s1a12waitCompleteySbSbF")
        decoy = self.find_symbol(target, "$s1a9lynxWriteyyYaF")

        # GetName() is the fully demangled name, which spells out "async".
        # GetDisplayName() drops it.
        self.assertNotIn("async", sync.GetName())
        self.assertIn("async", decoy.GetName())

        # 1. waitComplete's last instruction is a call, so its return address
        #    lands one past the end of the function.
        instructions = sync.GetInstructions(target)
        self.assertGreater(instructions.GetSize(), 0)
        last = instructions.GetInstructionAtIndex(instructions.GetSize() - 1)
        self.assertEqual(last.GetMnemonic(target), "bl")

        # 2. The async decoy occupies exactly that address.
        self.assertEqual(
            sync.GetEndAddress().GetLoadAddress(target),
            decoy.GetStartAddress().GetLoadAddress(target),
        )

    def find_symbol(self, target, mangled_name):
        symbols = target.FindSymbols(mangled_name)
        self.assertEqual(symbols.GetSize(), 1, "one symbol named %s" % mangled_name)
        return symbols.GetContextAtIndex(0).GetSymbol()
