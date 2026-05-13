import MyLib

func f() {
  let variable: AnyObject = MyDerived()
  print("break here \(variable)")
}

f()
