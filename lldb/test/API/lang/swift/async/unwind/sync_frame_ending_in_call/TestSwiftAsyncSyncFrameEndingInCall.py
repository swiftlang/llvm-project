"""
Test that a sync frame whose last instruction is a call is not mistaken for an
async frame.

Such a frame's return address is one past the end of the function, i.e. the first
byte of whichever function was placed next.  When that neighbour is an async
funclet, resolving the return address without backing it up by one makes
SwiftLanguageRuntime::GetRuntimeUnwindPlan build an async unwind plan -- CFA
taken from the async context register -- for a plain sync frame.  The unwind then
stops dead at that frame and every caller above it is lost.
"""

import lldb
import json
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil


class TestSwiftAsyncSyncFrameEndingInCall(TestBase):
    NO_DEBUG_INFO_TESTCASE = True

    @skipIfLLVMTargetMissing("AArch64")
    def test(self):
        """Test that no frames are lost above a sync frame ending in a call"""
        target = self.dbg.CreateTarget("")
        exe = "binary.json"
        with open(exe) as f:
            exe_uuid = json.load(f)["uuid"]

        target.AddModule(exe, "", exe_uuid)
        self.assertTrue(target.IsValid())

        core = self.getBuildArtifact("core")
        self.yaml2macho_core("arm64-sync-frame-ending-in-call.yaml", core, exe_uuid)

        process = target.LoadCore(core)
        self.assertTrue(process.IsValid())

        if self.TraceOn():
            self.runCmd("target modules dump symtab")
            self.runCmd("thread backtrace --unfiltered")

        thread = process.GetThreadAtIndex(0)
        self.assertTrue(thread.IsValid())

        stackframe_names = [
            "$s1a12reportAndDies5NeverOyF",
            "$s1a7doPanicyys6UInt32VF",
            "$s1a12waitCompleteySbSbF",
            "$s1a6calleryyF",
        ]
        self.assertEqual(thread.GetNumFrames(), len(stackframe_names))
        for i, name in enumerate(stackframe_names):
            self.assertEqual(
                name, thread.GetFrameAtIndex(i).GetSymbol().GetMangledName()
            )
