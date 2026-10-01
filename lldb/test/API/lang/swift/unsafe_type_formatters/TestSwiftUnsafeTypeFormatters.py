"""
Test that Swift unsafe types get formatted properly
"""

import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftUnsafeTypeFormatters(TestBase):

    def continue_to(self, process, marker):
        threads = lldbutil.continue_to_source_breakpoint(
            self, process, marker, lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        return threads[0].GetFrameAtIndex(0)

    def var(self, frame, path):
        return frame.GetValueForVariablePath(path, lldb.eDynamicCanRunTarget)

    def check_buffer(self, buf, typename, count):
        lldbutil.check_variable(self, buf, typename=typename, num_children=count)
        # The summary includes the buffer's base address.
        values = "1 value" if count == 1 else "%d values" % count
        self.assertRegex(buf.GetSummary(), r"^%s \(0x[0-9a-f]+\)$" % values)

    def check_pointer(self, ptr, typename):
        lldbutil.check_variable(self, ptr, typename=typename)
        self.assertRegex(ptr.GetValue(), r"^0x[0-9a-f]+$")

    def check_rgb(self, color):
        lldbutil.check_variable(self, color, value="RGB")
        rgb = color.GetChildMemberWithName("RGB")
        for i, component in enumerate(["155", "219", "255"]):
            lldbutil.check_variable(self, rgb.GetChildAtIndex(i), value=component)

    def check_hex(self, color):
        lldbutil.check_variable(self, color, value="Hex")
        lldbutil.check_variable(self, color.GetChildMemberWithName("Hex"),
                                value="4539903")

    def check_bytes(self, buf, typename, values):
        self.check_buffer(buf, typename, len(values))
        for i, value in enumerate(values):
            lldbutil.check_variable(self, buf.GetChildAtIndex(i), value=value)

    @skipEmbeddedSwift
    @swiftTest
    def test(self):
        """Test that Swift unsafe pointer and buffer types get formatted properly"""
        self.build()
        _, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break 01", lldb.SBFileSpec("main.swift"))
        buf = self.var(thread.GetFrameAtIndex(0), "buf")
        self.check_buffer(buf, "Swift.UnsafeBufferPointer<a.IntPair>", 3)
        for i, (original, opposite) in enumerate([("1", "-1"), ("-2", "2"), ("3", "-3")]):
            pair = buf.GetChildAtIndex(i)
            lldbutil.check_variable(self, pair.GetChildMemberWithName("original"),
                                    value=original)
            lldbutil.check_variable(self, pair.GetChildMemberWithName("opposite"),
                                    value=opposite)

        for marker, state in [("break 02", "Off"), ("break 03", "On")]:
            mutbuf = self.var(self.continue_to(process, marker), "mutbuf")
            self.check_buffer(mutbuf, "Swift.UnsafeMutableBufferPointer<a.Toggle>", 1)
            lldbutil.check_variable(self, mutbuf.GetChildAtIndex(0), value=state)

        frame = self.continue_to(process, "break 04")
        ptr = self.var(frame, "unsafe_ptr")
        self.check_pointer(ptr, "Swift.UnsafePointer<a.ColorCode>")
        self.check_rgb(ptr.GetChildMemberWithName("pointee"))
        self.check_rgb(self.var(frame, "unsafe_ptr.pointee"))

        frame = self.continue_to(process, "break 05")
        ptr = self.var(frame, "unsafe_mutable_ptr")
        self.check_pointer(ptr, "Swift.UnsafeMutablePointer<a.ColorCode>")
        self.check_hex(ptr.GetChildMemberWithName("pointee"))
        self.check_hex(self.var(frame, "unsafe_mutable_ptr.pointee"))

        frame = self.continue_to(process, "break 06")
        self.check_pointer(self.var(frame, "unsafe_raw_ptr"), "Swift.UnsafeRawPointer")

        buf = self.var(self.continue_to(process, "break 07"), "buf")
        self.check_buffer(buf, "Swift.UnsafeBufferPointer<a.ColorCode>", 2)
        self.check_rgb(buf.GetChildAtIndex(0))
        self.check_hex(buf.GetChildAtIndex(1))

        mutbuf = self.var(self.continue_to(process, "break 08"), "mutbuf")
        self.check_buffer(mutbuf, "Swift.UnsafeMutableBufferPointer<a.Flyable>", 2)
        for i, fly in enumerate(['"🦅"', '"🛸"']):
            lldbutil.check_variable(
                self, mutbuf.GetChildAtIndex(i).GetChildMemberWithName("fly"), summary=fly)

        buf = self.var(self.continue_to(process, "break 09"), "buf")
        self.check_buffer(buf, "Swift.UnsafeBufferPointer<a.Number<Swift.Double>>", 2)
        lldbutil.check_variable(
            self, buf.GetChildAtIndex(0).GetChildMemberWithName("number_value"), value="42")
        number = buf.GetChildAtIndex(1).GetChildMemberWithName("number_value")
        self.assertRegex(number.GetValue(), r"^3\.14[0-9]*$")

        # The buffers hold the bytes 0 through 255.
        all_bytes = [str(i) for i in range(256)]
        frame = self.continue_to(process, "break 10")
        self.check_bytes(self.var(frame, "rawbuf"), "Swift.UnsafeRawBufferPointer",
                         all_bytes)
        frame = self.continue_to(process, "break 11")
        self.check_bytes(self.var(frame, "alias"), "ByteBuffer", all_bytes)
        frame = self.continue_to(process, "break 12")
        self.check_bytes(self.var(frame, "secondAlias"), "ByteBufferAlias", all_bytes)

        for marker, values in [("break 13", ["0", "1"]), ("break 14", ["1", "0"])]:
            frame = self.continue_to(process, marker)
            self.check_bytes(self.var(frame, "mutrawbuf"),
                             "Swift.UnsafeMutableRawBufferPointer", values)
        lldbutil.check_variable(self, frame.GetValueForVariablePath("mutrawbuf[0]"),
                                typename="Swift.UInt8", value="1")

        buffer = self.continue_to(process, "break 15").FindVariable("buffer")
        self.check_buffer(buffer, "Swift.UnsafeBufferPointer<a.NotRaw>", 3)
        for i, x in enumerate(["1", "2", "4"]):
            lldbutil.check_variable(
                self, buffer.GetChildAtIndex(i).GetChildMemberWithName("x"), value=x)
