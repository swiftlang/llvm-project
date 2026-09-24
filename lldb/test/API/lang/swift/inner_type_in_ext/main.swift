import ModBase

extension Outer {
  class Inner<Content> {
    let tag: Int = 42
  }
}

func f<T>(_: T) {
  let variable = Outer.Inner<T>()
  print("break here")  // break here
}

f(1)
