import Foundation

// The library is linked without local symbols, so the context descriptors of
// the private classes have no symbol of their own and sit right after the
// descriptor of the preceding public struct.

public struct Message {
  public var object: NSObject
  public init(object: NSObject) { self.object = object }
}

private class Leaf: NSObject { var c = 3 }

public struct CyclicMessage {
  public var object: NSObject
  public init(object: NSObject) { self.object = object }
}

private class Base: NSObject { var a = 1 }
private class Derived: Base { var b = 2 }

public func makeMessage() -> Message {
  _ = Leaf()
  return Message(object: NSObject())
}

public func makeCyclicMessage() -> CyclicMessage {
  _ = Derived()
  return CyclicMessage(object: NSObject())
}
