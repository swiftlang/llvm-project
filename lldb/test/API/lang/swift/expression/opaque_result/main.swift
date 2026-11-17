public protocol P {}

public struct S: P {
  public var x = 1
  public var y = "hello"
  @inlinable public init() {}
}

// The underlying type of an inlinable function's opaque result is visible to
// every client, including the expression evaluator.
@inlinable public func makeInlinable() -> some P {
  return S()
}

func main() {
  let s = makeInlinable()
  print(s) // break here
}

main()
