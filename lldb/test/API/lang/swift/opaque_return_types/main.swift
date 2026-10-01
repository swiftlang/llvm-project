protocol P {}

extension Int: P {}

@available(macOS 10.15, *)
func foo() -> some P { return 0 }

@available(macOS 10.15, *)
func genericOpaque<T: P>(_ x: T) -> some P { return x }

struct Wrapper<T: P>: P {
  var value: T

  @available(macOS 10.15, *)
  subscript(unwrapped _: ()) -> some P {
    return value
  }
  @available(macOS 10.15, *)
  subscript(wrapped _: ()) -> some P {
    return self
  }
}

@available(macOS 10.15, *)
func genericWrapOpaque<T: P>(_ x: T) -> some P {
  return Wrapper(value: genericOpaque(x))
}

if #available(macOS 10.15, *) {
  var prop: some P { return 0 }
  var wrapProp: some P { return Wrapper(value: prop) }

  func bar<T: P>(_ x: T) {
    print(x)
  }

  func main() {
    let a = foo()
    let b = genericOpaque(a)
    let c = genericOpaque(b)
    let d = genericWrapOpaque(c)
    let e = genericWrapOpaque(d)
    let f = prop
    let g = wrapProp
    let h = Wrapper(value: wrapProp)
    let i = h[unwrapped: ()]
    let j = h[wrapped: ()]

    bar(a) // break here
    bar(b)
    bar(c)
    bar(d)
    bar(e)
    bar(f)
    bar(g)
    bar(h)
    bar(i)
    bar(j)
  }

  main()
}
