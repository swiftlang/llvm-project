class C {
  var x = 42
}

func f() {
  let c = C()
  print(c.x) // break here
}

f()
