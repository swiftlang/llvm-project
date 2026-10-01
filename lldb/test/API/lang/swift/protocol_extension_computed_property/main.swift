import Foundation
#if canImport(CoreGraphics)
import CoreGraphics
#endif

extension Measurement where UnitType == UnitAngle {
  var radians: CGFloat {
    return CGFloat(self.converted(to: .radians).value)
  }

  var degrees: CGFloat {
    return CGFloat(self.converted(to: .degrees).value)
  }

  func f() {
    return // break here
  }
}

let measure = Measurement<UnitAngle>(value: 100, unit: UnitAngle.degrees)
measure.f()
