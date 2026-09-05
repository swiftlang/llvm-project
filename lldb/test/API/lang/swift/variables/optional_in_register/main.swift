var sideEffect = 0

@inline(never)
func callee() {
  sideEffect += 1 // break here
}

@inline(never)
func caller(_ optionalBytes: [UInt8]?, _ plainBytes: [UInt8]) -> Int {
  callee()
  return (optionalBytes?.count ?? 0) + plainBytes.count
}

sideEffect = caller([1, 2, 3], [4, 5, 6, 7])
sideEffect = caller(nil, [8, 9])
