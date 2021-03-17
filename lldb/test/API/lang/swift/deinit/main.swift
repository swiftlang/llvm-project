private class K {
  init() {}
}

private func readKRetInt(_ f: K) -> Int {
  return 2
}

func useInt(_ i: Int) {
  print(i)
}

public func main() {
  let a = K()
  let b : K? = K()
  let x = readKRetInt(a) // break here
  let y = readKRetInt(b!)
  useInt(x+y) // break here
}

main()
