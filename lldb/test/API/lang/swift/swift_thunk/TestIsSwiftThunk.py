import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil

class TestCase(lldbtest.TestBase):

    mydir = lldbtest.TestBase.compute_mydir(__file__)

    @swiftTest
    @skipIf(oslist=['windows', 'linux'])
    def test(self):
        """Test SBFrame.IsSwiftThunk()"""
        self.build()

        # `breaker` is called on a protocol, which causes execution to run
        # through a protocol witness thunk. The first stop is that thunk.
        _, process, thread, _ = lldbutil.run_to_name_breakpoint(self, "breaker")
        frame = thread.frames[0]
        self.assertTrue(frame.IsSwiftThunk())

        # Proceed to the real implementation.
        process.Continue()
        frame = thread.frames[0]
        self.assertEqual(frame.function.GetDisplayName(), "S.breaker()")
        self.assertFalse(frame.IsSwiftThunk())
