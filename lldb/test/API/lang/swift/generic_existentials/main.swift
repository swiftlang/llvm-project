class MyClass {
  var x: Int
  init(_ x: Int) {
    self.x = x
  }
}

func f<T>(_ x : T) -> T {
  return x // break 1
}

f(MyClass(23) as Any)
f(MyClass(23) as AnyObject)

func g<T>(_ x : T) -> T {
  return x // break 2
}

struct MyStruct {
  var x: Int

  init(_ x: Int) {
    self.x = x
  }
}

g(MyStruct(23) as Any)

func h<T>(_ x : T) -> T {
  return x // break 3
}

struct MyBigStruct {
  var x: Int
  var y: Int
  var z: Int
  var w: Int

  init(_ x: Int) {
    self.x = x
    self.y = x + 1
    self.z = x + 2
    self.w = x + 3
  }
}

h(MyBigStruct(23) as Any)
