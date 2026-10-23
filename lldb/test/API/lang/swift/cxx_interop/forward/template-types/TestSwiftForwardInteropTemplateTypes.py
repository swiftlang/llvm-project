
"""
Test that a C++ class is visible in Swift.
"""
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *


class TestSwiftForwardInteropTemplateTypes(TestBase):

    # With explicitly built modules, ClangImporter cannot find a class template
    # specialization that no typedef names.
    @skipIf(swift_module_importer=no_match("noclang"))
    @swiftClangImporterTest
    @skipEmbeddedSwiftOnWindows
    def test(self):
        self.build()
        
        _, _, _, _= lldbutil.run_to_source_breakpoint(
            self, 'Set breakpoint here', lldb.SBFileSpec('main.swift'))

        self.expect('frame var wrapped', substrs=['Wrapper<CxxClass>', 't', 'a1', '10', 'a2', '20', 'a3', '30'])

        self.expect('expr wrapped', substrs=['Wrapper<CxxClass>', 't', 'a1', '10', 'a2', '20', 'a3', '30'])
