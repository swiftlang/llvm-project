"""
Test that a struct whose nominal type descriptor symbol is followed by the
unsymbolicated context descriptors of private ObjC-rooted classes is not
mistaken for one of these classes.

Images in the dyld shared cache have no local symbols. When LLDB attributed
an address inside a symbol to that symbol, the private classes' field
descriptors replaced the struct's own field descriptor in the reflection
cache.
"""
import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftStrippedPrivateTypeDescriptors(TestBase):
    def check_message(self, name):
        msg = self.frame().FindVariable("msg")
        self.assertEqual(msg.GetTypeName(), "Lib." + name)
        self.assertEqual(msg.GetNumChildren(), 1)
        child = msg.GetChildAtIndex(0)
        self.assertEqual(child.GetName(), "object")
        self.assertIn("NSObject", child.GetTypeName())

        stream = lldb.SBStream()
        self.assertTrue(msg.GetDescription(stream))
        self.assertIn("object = ", stream.GetData())

    @requireNotEmbeddedSwift
    @skipUnlessDarwin
    @swiftTest
    def test(self):
        self.build()
        # Which of the field descriptors sharing a name wins depends on the
        # order in which they are cached. Keep a persistent cache out of it.
        self.runCmd("settings set symbols.enable-swift-metadata-cache false")
        target, process, _, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        # Before the fix, `msg` was laid out like the private class `Leaf`.
        self.check_message("Message")

        # Before the fix, the private class `Derived` and its superclass
        # `Base` both had the name `CyclicMessage`, and computing the layout
        # of `msg` overflowed the stack.
        lldbutil.continue_to_source_breakpoint(
            self, process, "break cyclic", lldb.SBFileSpec("main.swift")
        )
        self.check_message("CyclicMessage")
