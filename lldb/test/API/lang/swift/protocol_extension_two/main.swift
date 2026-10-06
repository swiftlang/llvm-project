protocol Tinky {}

struct Winky : Tinky {
  var x : Int
}

extension Patatino where T == Winky {
  var baciotto : Int {
    return 0
  }

  func f() {
    return // break here
  }
}

struct Patatino<T> where T : Tinky {
  let x : T
}

let pat = Patatino<Winky>(x: Winky(x: 23))
print(pat.baciotto) // Use it so it isn't optimized out in embedded swift.
pat.f()
