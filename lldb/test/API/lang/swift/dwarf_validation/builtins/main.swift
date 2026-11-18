class Foo {
  var x = 1
}

struct Bar {
  var a: Int
  var b: Foo
}

func use(_ foo: Foo, _ bar: Bar) {
  print(foo.x, bar.a) // break here
}

let foo = Foo()
use(foo, Bar(a: 1, b: foo))
