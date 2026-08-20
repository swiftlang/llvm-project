import lldbsuite.test.lldbinline as lldbinline
from lldbsuite.test.decorators import *

# rdar://185128962 (Embedded Swift: po falls back to p, so object-description output is unavailable)
lldbinline.MakeInlineTest(__file__, globals(), decorators=[skipEmbeddedSwift, swiftTest])
