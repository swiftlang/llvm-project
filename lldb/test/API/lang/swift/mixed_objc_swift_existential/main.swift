import Foundation

@objc protocol ObjCProto {}

protocol ClassConstrainedProto: AnyObject {
  var payload: Int { get }
}

protocol UnconstrainedProto {
  var payload: Int { get }
}

final class Concrete: ObjCProto, ClassConstrainedProto, UnconstrainedProto {
  var payload: Int
  init(_ p: Int) {
    self.payload = p
  }
}

func entry() {
  // @objc protocol + a Swift protocol that is independently class-constrained.
  let classConstrained: ObjCProto & ClassConstrainedProto = Concrete(42)
  // @objc protocol + a Swift protocol that is NOT independently
  // class-constrained; the @objc member still class-constrains the existential.
  let unconstrained: ObjCProto & UnconstrainedProto = Concrete(43)
  // The existential metatype of the mixed composition.
  let metatype: (ObjCProto & UnconstrainedProto).Type = Concrete.self
  print("break here")
  _ = classConstrained
  _ = unconstrained
  _ = metatype
}

entry()
