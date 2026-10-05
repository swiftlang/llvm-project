import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestProtocolExtensionComputedProperty(TestBase):

    @requireNotEmbeddedSwift
    @swiftTest
    @skipUnlessDarwin
    @expectedFailureAll(
        bugnumber="rdar://60396797",
        setting=("symbols.use-swift-clangimporter", "false"),
    )
    def test(self):
        """Test evaluating self and a computed property in a constrained extension"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        options = lldb.SBExpressionOptions()
        radians = frame.EvaluateExpression("self.radians", options)
        self.assertSuccess(radians.GetError())
        lldbutil.check_variable(self, radians, typename="CoreGraphics.CGFloat",
                                summary="1.7453292519943295")
        measurement = frame.EvaluateExpression("self", options)
        self.assertSuccess(measurement.GetError())
        lldbutil.check_variable(
            self, measurement,
            typename="Foundation.Measurement<Foundation.UnitAngle>")
