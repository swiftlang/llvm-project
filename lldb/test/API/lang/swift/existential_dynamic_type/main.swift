protocol P1 {
  var a: Int { get }
}

protocol P2 {
  var b: Int { get }
}

protocol Composed: P1 & P2 {
  var c: Int { get }
}

struct S: Composed {
  var a = 1
  var b = 2
  var c = 3
}

// The parameter types below are implicit existentials, i.e. they are the same
// as `any P1`, `any P1 & P2` and `any Composed`.
func f(single: P1, composition: P1 & P2, inherited: Composed, anything: Any) {
  print(single.a, composition.b, inherited.c, anything) // break here
}

f(single: S(), composition: S(), inherited: S(), anything: 42)
