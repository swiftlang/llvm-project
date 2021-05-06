protocol P {
  func breaker() -> Int
}

struct S: P {
  func breaker() -> Int { 41 }
}

let p: P = S()
print(p.breaker())
