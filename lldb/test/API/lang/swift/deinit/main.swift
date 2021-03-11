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
  let b = K()
  let y = readKRetInt(b)
  useInt(y) // break here
}

main()
