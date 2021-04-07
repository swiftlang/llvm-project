func randInt(_ i: Int) async -> Int {
  return Int.random(in: 1...i)
}

func use<T>(_ t: T...) {}

@main struct Main {
  static func main() async {
    let a = await randInt(30)
    let b = await randInt(a + 11)
    use(a, b)
  }
}
