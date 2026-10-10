"""
Test variables whose type contains an opaque archetype LLDB cannot resolve.

When the opaque type descriptor cannot be found, the archetype's TypeRef holds
the descriptor's symbol name instead of a type mangling and has no demangle
tree, so every type that encloses it must cope with a missing child.
"""

import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftUnresolvedOpaqueArchetype(TestBase):
    NO_DEBUG_INFO_TESTCASE = True

    # Stripping the library's local symbols emulates a framework in the dyld
    # shared cache.
    @requireDarwin
    @requireNotEmbeddedSwift
    @swiftTest
    def test(self):
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"),
            extra_images=["OpaqueLib"])
        frame = thread.GetSelectedFrame()

        # Each variable encloses the archetype in a different kind of type or
        # at a different depth.
        for name in ["box", "either", "opt", "arr", "dict", "pack", "tuple",
                     "nested", "two", "deep"]:
            variable = frame.FindVariable(name)
            self.assertTrue(variable.IsValid(), name)
            # Laying out the type requires resolving the archetype.
            self.assertEqual(variable.GetByteSize(), 0, name)
            self.assertTrue(variable.GetError().Fail(), name)
            self.assertIn("opaque return type of", variable.GetTypeName(), name)
            self.assertEqual(variable.GetValueType(),
                             lldb.eValueTypeVariableLocal, name)

        # A class instance points to its own metadata, which names the
        # underlying type.
        ref = frame.FindVariable("ref")
        self.assertSuccess(ref.GetError())
        self.assertIn("OpaqueLib.Wrap", ref.GetTypeName())
