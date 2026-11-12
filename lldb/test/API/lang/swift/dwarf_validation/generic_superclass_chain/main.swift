final class Ref {
  var x = 7
}

class Base {
  var a = 1
}

class Middle<T>: Base {
  var t: T
  init(_ t: T) {
    self.t = t
  }
}

// A class whose superclass is a specialization of a generic class that itself
// has a superclass.
final class Leaf: Middle<Ref> {
  var b = 2
}

func use<T>(_ t: T) {}

func main() {
  let leaf = Leaf(Ref())
  use(leaf) // break here
}

main()
