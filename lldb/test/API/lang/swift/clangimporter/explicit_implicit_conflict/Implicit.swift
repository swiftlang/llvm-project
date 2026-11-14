import Z

public func implicitFunc() {
  let z = ZStruct(z: 1, s: Shared(x: 2))
  print(z) // break implicit
}
