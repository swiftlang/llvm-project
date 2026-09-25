func parked() async -> Int {
  await withUnsafeContinuation { (_: UnsafeContinuation<Void, Never>) in }
  return 0
}

func nestedParent() async -> Int {
  async let x = parked()
  async let y = parked()
  return await x + y
}

@main struct Main {
  static func main() async {
    let unstructured = Task { () -> Int in
      // On a single-threaded executor, the yields let every other task run
      // until it suspends.
      await Task.yield()
      await Task.yield()
      print("break here")
      return 1
    }
    async let a = parked()
    async let b = nestedParent()
    await withTaskGroup(of: Int.self) { group in
      group.addTask { await parked() }
      group.addTask { await parked() }
      group.addTask { await unstructured.value }
      _ = await unstructured.value
    }
    _ = await a + b
  }
}
