// The debug info type of `kp` is
// WritableKeyPath<f() -> [Int].(S), [[Int]]>, where the [[Int]] is
// spelled with the debugger-only "sugared array" mangling, while the [Int]
// in the enclosing function's return value is spelled out as Array<Int>.
func f() -> [Int] {
  struct S {
    var values: [[Int]] = [[1, 2]]
  }
  let kp = \S.values
  let s = S()
  // Array has no CustomStringConvertible conformance in embedded Swift.
  print(s[keyPath: kp][0][0]) // break here
  return [0]
}

_ = f()
