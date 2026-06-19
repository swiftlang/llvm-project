
"""
Test that evaluating expressions works on forward interop mode.
"""
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *

class TestSwiftForwardInteropExpressions(TestBase):

    def setup(self, bkpt_str):
         self.build()
         self.runCmd('log enable lldb types expr -v')
         _, _, thread, _ = lldbutil.run_to_source_breakpoint(
             self, bkpt_str, lldb.SBFileSpec('main.swift'))
         return thread

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_return_void_1(self):
        self.setup('Break here')
        self.expect('expr returnsVoid()', substrs=['(Void)'])

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_return_void_2(self):
        self.setup('Break here')
        self.expect('expr returnsVoid()', substrs=['(Void)'])

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_return_void_3(self):
        self.setup('Break here')
        self.expect('expr returnsVoid()', substrs=['(Void)'])

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_return_void_4(self):
        self.setup('Break here')
        self.expect('expr returnsVoid()', substrs=['(Void)'])

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_return_void_5(self):
        self.setup('Break here')
        self.expect('expr returnsVoid()', substrs=['(Void)'])

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_return_void_6(self):
        self.setup('Break here')
        self.expect('expr returnsVoid()', substrs=['(Void)'])

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_return_void_7(self):
        self.setup('Break here')
        self.expect('expr returnsVoid()', substrs=['(Void)'])

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_return_void_8(self):
        self.setup('Break here')
        self.expect('expr returnsVoid()', substrs=['(Void)'])

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_return_void_9(self):
        self.setup('Break here')
        self.expect('expr returnsVoid()', substrs=['(Void)'])

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_return_void_10(self):
        self.setup('Break here')
        self.expect('expr returnsVoid()', substrs=['(Void)'])

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_throw_exception1(self):
        self.setup('Break here')
        self.expect('expr throwException()', substrs=['internal c++ exception breakpoint'], error=True)

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_throw_exception2(self):
        self.setup('Break here')
        self.expect('expr throwException()', substrs=['internal c++ exception breakpoint'], error=True)

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_throw_exception3(self):
        self.setup('Break here')
        self.expect('expr throwException()', substrs=['internal c++ exception breakpoint'], error=True)

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_throw_exception4(self):
        self.setup('Break here')
        self.expect('expr throwException()', substrs=['internal c++ exception breakpoint'], error=True)

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_throw_exception5(self):
        self.setup('Break here')
        self.expect('expr throwException()', substrs=['internal c++ exception breakpoint'], error=True)

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_throw_exception6(self):
        self.setup('Break here')
        self.expect('expr throwException()', substrs=['internal c++ exception breakpoint'], error=True)

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_throw_exception7(self):
        self.setup('Break here')
        self.expect('expr throwException()', substrs=['internal c++ exception breakpoint'], error=True)

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_throw_exception8(self):
        self.setup('Break here')
        self.expect('expr throwException()', substrs=['internal c++ exception breakpoint'], error=True)

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_throw_exception9(self):
        self.setup('Break here')
        self.expect('expr throwException()', substrs=['internal c++ exception breakpoint'], error=True)

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_throw_exception10(self):
        self.setup('Break here')
        self.expect('expr throwException()', substrs=['internal c++ exception breakpoint'], error=True)


    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_throw_exception_21(self):
        self.setup('Break here')
        self.expect('expr throwException2()', substrs=['internal c++ exception breakpoint'], error=True)

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_throw_exception_22(self):
        self.setup('Break here')
        self.expect('expr throwException2()', substrs=['internal c++ exception breakpoint'], error=True)

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_throw_exception_23(self):
        self.setup('Break here')
        self.expect('expr throwException2()', substrs=['internal c++ exception breakpoint'], error=True)

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_throw_exception_24(self):
        self.setup('Break here')
        self.expect('expr throwException2()', substrs=['internal c++ exception breakpoint'], error=True)

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_throw_exception_25(self):
        self.setup('Break here')
        self.expect('expr throwException2()', substrs=['internal c++ exception breakpoint'], error=True)

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_throw_exception_26(self):
        self.setup('Break here')
        self.expect('expr throwException2()', substrs=['internal c++ exception breakpoint'], error=True)

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_throw_exception_27(self):
        self.setup('Break here')
        self.expect('expr throwException2()', substrs=['internal c++ exception breakpoint'], error=True)

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_throw_exception_28(self):
        self.setup('Break here')
        self.expect('expr throwException2()', substrs=['internal c++ exception breakpoint'], error=True)

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_throw_exception_29(self):
        self.setup('Break here')
        self.expect('expr throwException2()', substrs=['internal c++ exception breakpoint'], error=True)

    @skipIfLinux # rdar://106871422"
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false')) # rdar://106871275
    @swiftTest
    def test_throw_exception_210(self):
        self.setup('Break here')
        self.expect('expr throwException2()', substrs=['internal c++ exception breakpoint'], error=True)
