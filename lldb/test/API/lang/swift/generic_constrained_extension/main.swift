protocol P {
  associatedtype A
  func make() -> A
}

struct S: P {
  func make() -> Int { 42 }
}

struct Box<V> { var v: V }

class C<T> {
  let t: T
  init(_ t: T) { self.t = t }
}

// The requirement T: P, and with it T.A, exists only in this extension, not
// in the generic signature of C itself.
extension C where T: P {
  func method() {
    let local = t.make()
    let box = Box(v: local)
    print(local, box) // break in class extension
  }
}

extension Array where Element: P {
  func method() {
    let local = self[0].make()
    print(local) // break in array extension
  }
}

C(S()).method()
[S()].method()
