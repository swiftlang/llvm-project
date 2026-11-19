public protocol Holder {}

class Storage<T> {
  let value: T
  init(_ value: T) { self.value = value }
}

public struct Box<T>: Holder {
  let storage: Storage<T>
  public var value: T { storage.value }
  public init(_ value: T) { storage = Storage(value) }
}

public class Container {
  public init() {}

  public static func make<A>(_ x: A) -> some Holder {
    return Box<A>(x)
  }
}
