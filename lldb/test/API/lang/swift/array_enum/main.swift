enum x : String {
  case patatino
}

struct y {
  var z: x?
}

func main() -> Int {
  var a = y()
  a.z = x.patatino
  var j = [a]
  return 0 // break here
}

let _ = main()
