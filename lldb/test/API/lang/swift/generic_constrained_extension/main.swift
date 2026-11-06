protocol P {
  associatedtype A
  func make() -> A
}

struct S: P {
  func make() -> Int { 42 }
}

struct Box<V> { var v: V }

// A generic type with a requirement of its own.
class Wrapper<U: P> {
  let u: U
  init(_ u: U) { self.u = u }
}

struct PackWrapper<each U: P> {
  let u: (repeat each U)
  init(_ u: repeat each U) { self.u = (repeat each u) }
}

class C<T> {
  let t: T
  init(_ t: T) { self.t = t }

  func whereClause() where T: P {
    let wrapper = Wrapper(t)
    print(wrapper) // break in where clause
  }
}

// The requirement T: P, and with it T.A, exists only in this extension, not
// in the generic signature of C itself.
extension C where T: P {
  func method() {
    let local = t.make()
    let box = Box(v: local)
    print(local, box) // break in class extension
  }

  func wrapperMethod() {
    let wrapper = Wrapper(t)
    print(wrapper) // break in wrapper method
  }

  func packWrapperMethod() {
    let packWrapper = PackWrapper(t)
    print(packWrapper) // break in pack wrapper method
  }
}

// Here T: P is part of the generic signature of the type itself.
class ConstrainedC<T: P> {
  let t: T
  init(_ t: T) { self.t = t }

  func method() {
    let wrapper = Wrapper(t)
    print(wrapper) // break in constrained type
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
C(S()).whereClause()
ConstrainedC(S()).method()
C(S()).packWrapperMethod()
C(S()).wrapperMethod()
