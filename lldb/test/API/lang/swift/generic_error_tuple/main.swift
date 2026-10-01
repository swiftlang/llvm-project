class PayloadErr : Error {
  var x : Int

  init(_ x : Int) {
    self.x = x
  }
}

class MyOtherErr : PayloadErr {}

enum CErr : Error {
  case Topolino
  case Paperino
}

func g<T, U>(_ tuple : (T, U)) -> T {
  return tuple.0 // break 1
}

func h<U, V>(_ tuple : (U, V)) -> (U, V) {
  return tuple // break 2
}

g((CErr.Topolino as Error, 42))
h((PayloadErr(23), MyOtherErr(42) as PayloadErr))
