class Generic<T> {
  var t: T
  init(_ t: T) { self.t = t }
  func method() {
    print(t) // break in generic method
  }
}

class OtherGeneric<U> {
  var u: U
  init(_ u: U) { self.u = u }
  func method() {
    print(u) // break in other generic method
  }
}

func nonGeneric() {
  let i = 1
  print(i) // break in non-generic function
}

Generic(42).method()
OtherGeneric("hello").method()
nonGeneric()
