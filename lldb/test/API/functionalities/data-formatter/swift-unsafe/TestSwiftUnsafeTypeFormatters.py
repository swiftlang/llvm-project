"""
Test that Swift unsafe types get formatted properly
"""
import lldbsuite.test.lldbinline as lldbinline
from lldbsuite.test.decorators import *

# Skip test until rdar://109831415 is fixed. After moving to opaque pointer
# usage this test fails.
lldbinline.MakeInlineTest(__file__, globals(), decorators=[swiftTest,
                                                           skipIfDarwin,
                                                           skipIfLinux])
