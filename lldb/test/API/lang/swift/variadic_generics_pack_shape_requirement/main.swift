struct Container<each T> {
  let items: (repeat each T)

  init(_ items: repeat each T) {
    self.items = (repeat each items)
  }
}

extension Container {
  // A variadic generic method of a variadic generic type. Its generic
  // signature has a pack shape requirement (τ_0_0.shape == τ_1_0.shape),
  // and the metadata pack for the outer pack `each T` is described by a
  // debug variable.
  func process<each U>(transform: repeat (each T) -> each U) -> (repeat each U) {
    let result = (repeat (each transform)(each items))
    return result // break here
  }
}

let many = Container(1, "hello", 3.14)
_ = many.process(transform: { $0 + 1 }, { $0 + "!" }, { $0 * 2 })
