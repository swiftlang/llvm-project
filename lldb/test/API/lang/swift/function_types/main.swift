class C {
  init(x: Int) {}
  func method(_ a: Int) -> String { return "" }
  static func staticMethod() -> Int { return 1 }
  var computed: Int {
    return 2 // break in getter
  }
}

struct S {
  var x = 0
  mutating func mutatingMethod() -> Int {
    x += 1
    return x
  }
}

func noArguments() -> Int { return 3 }
func twoArguments(_ a: Int, _ b: String) -> Bool { return true }
func throwing(_ x: Int) throws -> Int { return x }
func generic<T>(_ t: T) -> T { return t }
func returnsClosure() -> (Int) -> Int {
  return { x in
    return x + 1 // break in closure
  }
}

func main() {
  let c = C(x: 1)
  _ = c.method(1)
  _ = C.staticMethod()
  _ = c.computed
  var s = S()
  _ = s.mutatingMethod()
  _ = noArguments()
  _ = twoArguments(1, "")
  _ = try! throwing(1)
  _ = generic(1)
  _ = returnsClosure()(1)
}

main()
