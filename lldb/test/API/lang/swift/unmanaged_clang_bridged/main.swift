import Foo

@inline(never)
func makeNil() -> Unmanaged<MyBridged>? { return nil }

func f() {
  let nilU: Unmanaged<MyBridged>? = makeNil()
  let p = UnsafeMutableRawPointer(bitPattern: 0xdeadbeef)!
  let someU: Unmanaged<MyBridged> = Unmanaged.fromOpaque(p)
  let optU: Unmanaged<MyBridged>? = Unmanaged.fromOpaque(p)
  print("break here", nilU as Any)
}

f()
