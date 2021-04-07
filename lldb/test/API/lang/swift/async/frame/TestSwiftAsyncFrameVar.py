import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil

class TestCase(lldbtest.TestBase):

    mydir = lldbtest.TestBase.compute_mydir(__file__)

    @swiftTest
    @skipIf(oslist=['windows', 'linux'])
    def test(self):
        """Test `frame variable` in async functions"""
        self.build()

	# Setting a breakpoint on "main" results in a breakpoint at the start
	# of each coroutine "funclet" function.
        _, process, _, _ = lldbutil.run_to_name_breakpoint(self, 'main')

        while process.state == lldb.eStateStopped:
            frame = process.GetSelectedThread().frames[0]
            symbol = frame.symbol.mangled
            if symbol == "$s1a4MainV4mainyyYFZTQ0_":
                a = frame.FindVariable("a")
                self.assertTrue(a.IsValid())
                # self.assertGreater(a.unsigned, 0)
                self.assertEqual(a.unsigned, 0)
            elif symbol == "$s1a4MainV4mainyyYFZTQ1_":
                a = frame.FindVariable("a")
                self.assertTrue(a.IsValid())
                # self.assertGreater(a.unsigned, 0)
                self.assertEqual(a.unsigned, 0)
                b = frame.FindVariable("b")
                self.assertTrue(b.IsValid())
                # self.assertGreater(b.unsigned, 0)
                self.assertEqual(a.unsigned, 0)

            process.Continue()
