# TestSwiftNSArrayCodeRunningFormatter.py
#
# This source file is part of the Swift.org open source project
#
# Copyright (c) 2014 - 2016 Apple Inc. and the Swift project authors
# Licensed under Apache License v2.0 with Runtime Library Exception
#
# See https://swift.org/LICENSE.txt for license information
# See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
#
# ------------------------------------------------------------------------------
import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftNSArrayCodeRunningFormatter(TestBase):

    @requireNotEmbeddedSwift
    @swiftTest
    @requireObjCFoundation
    @skipIf(macos_version=["=", "15.0"])
    def test(self):
        """Test formatting of a Swift subclass of NSArray and of arrays bridged from it"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        t = frame.FindVariable("t", lldb.eDynamicCanRunTarget)
        self.assertRegex(t.GetValue(), r"^0x[0-9a-f]+$")
        nsarray = t.GetChildAtIndex(0)
        lldbutil.check_variable(self, nsarray, typename="Foundation.NSArray")
        lldbutil.check_variable(self, nsarray.GetChildAtIndex(0),
                                typename="ObjectiveC.NSObject")
        ta = frame.FindVariable("ta", lldb.eDynamicCanRunTarget)
        lldbutil.check_variable(self, ta, summary="<uninitialized>")
        storage = frame.GetValueForVariablePath("ta._buffer._storage.rawValue",
                                                lldb.eDynamicCanRunTarget)
        self.assertRegex(storage.GetValue(), r"^0x[0-9a-f]+$")
        tb = frame.FindVariable("tb")
        lldbutil.check_variable(self, tb, summary="1 value")
        lldbutil.check_variable(self, tb.GetChildAtIndex(0), use_dynamic=True,
                                summary='"abc"')
        self.expect('po t', substrs=['0 : abc'])
        self.expect('po ta', substrs=['0 : abc'])
        self.expect('po tb', substrs=['0 : abc'])
