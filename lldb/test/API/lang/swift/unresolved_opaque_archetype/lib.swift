public protocol P {
  func f() -> Int
}

struct Impl: P {
  var a = 11
  var b = 22.5
  func f() -> Int { a }
}

struct Wrap<Inner: P>: P {
  var inner: Inner
  var tag = 7
  func f() -> Int { inner.f() + tag }
}

// Internal, so its opaque type descriptor is a hidden symbol that the
// Makefile's `strip -x` removes -- the same situation as a framework in the
// dyld shared cache, which carries no local symbols.
func innerOpaque() -> some P { Impl() }

// Public, so this descriptor survives stripping, but the underlying type it
// points at is the archetype above, which can no longer be resolved.
public func makeOpaque() -> some P { Wrap(inner: innerOpaque()) }
