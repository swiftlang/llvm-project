
"""
Test that evaluating expressions works on forward interop mode.
"""
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *


class TestSwiftForwardInteropExpressions(TestBase):

    def setup(self, bkpt_str):
        self.build()

        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, bkpt_str, lldb.SBFileSpec("main.swift")
        )
        return thread

    @skipIfLinux  # rdar://106871422"
    @skipIfWindows  # MSVC exception ABI: no type_info vftable in the target.
    @skipIf(setting=("symbols.use-swift-clangimporter", "false"))  # rdar://106871275
    @skipEmbeddedSwift  # Hangs in CI; `po` also has no reflection to fall back on.
    @swiftTest
    def test(self):
        self.setup("Break here")

        types_log = self.getBuildArtifact("types.log")
        self.expect("log enable lldb types -v -f " + types_log)
        # Check that we can call free functions.
        self.expect('expr returnsInt()', substrs=['CInt', '42'])

        # Check that we can call unused free functions.
        self.expect('expr returnsIntUnused()', substrs=['CInt', '37'])

        # Check that we can call a C++ constructor.
        self.expect('expr CxxClass()', substrs=['CxxClass', 'a = 100', 'b = 101'])

        # Check that we can call methods.
        self.expect('expr cxxClass.sum()', substrs=['CInt', '201'])

        # Check that we can access a C++ type's ivars
        self.expect('expr cxxClass.a', substrs=['CInt', '100'])
        self.expect('expr cxxSubclass.a', substrs=['CInt', '100'])
        self.expect('expr cxxSubclass.c', substrs=['CInt', '102'])

        # Check that calling a function that throws an exception fails on expression evaluation
        self.expect('expr throwException()', substrs=['internal c++ exception breakpoint'], error=True)

        # Check that we can make persistent variables.
        self.expect('expr var $cxxClass = CxxClass()')

        # Check that we can refer to the persistent variable.
        self.expect('expr $cxxClass', substrs=['CxxClass', 'a = 100', 'b = 101'])

        # Check that we can call methods on the persistent variable.
        self.expect('expr $cxxClass.sum()', substrs=['CInt', '201'])

        # Check that po prints the fields of a base class
        self.expect('po cxxClass', substrs=['CxxClass', 'a : 100', 'b : 101'])

        self.filecheck_log(types_log, __file__)
        # CHECK: [CheckFlagInCU] Found flag -enable-experimental-cxx-interop in CU:

    @expectedFailureAll(bugnumber="rdar://106216567")
    @skipEmbeddedSwift  # Hangs in CI.
    @swiftTest
    def test_po_subclass(self):
        self.setup('Break here')

        self.expect('po CxxSubclass()', substrs=['CxxClass', 'a : 100', 'b : 101', 'c : 102'])
