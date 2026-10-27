"""
Test how Swift function names are printed in a backtrace.
"""
import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftFunctionNameBacktrace(TestBase):
    # foo_ dynamically casts its generic argument, which embedded Swift
    # rejects.
    @requireNotEmbeddedSwift
    @swiftTest
    @expectedFailureAll(debug_info=["pdb"])
    def test_function_names_in_backtrace(self):
        self.build()
        lldbutil.run_to_name_breakpoint(self, "qux")

        # foo_ is generic over T, and its substituted type argument is printed
        # module-qualified. The module is named after the executable, so match
        # the name rather than spelling it out.
        self.expect(
            "thread backtrace",
            patterns=[
                r"frame #0: .*`Baz\.qux<Int>\(a=1\)",
                r"frame #1: .*`static Foo\.foo_<\w+\.Baz>\(a=\(baz = 1\)\)",
                r"frame #2: .*`bar\(a=1, b=1\)",
                r"frame #3: .*`foo\(\)",
            ],
        )
