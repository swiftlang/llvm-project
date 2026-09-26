var sideEffect = 0

@inline(never)
func consume(_ x: Int) {
  if x == Int.min { sideEffect += 1 }
}

@inline(never)
func callee() {
  sideEffect += 1 // break here
}

@inline(never)
func caller(_ a: Int, _ b: Int) -> Int {
  // a and b are dead after this call
  consume(a &+ b)
  callee()
  return sideEffect // keeps callee() from being a tail call
}

sideEffect = caller(11, 22)
