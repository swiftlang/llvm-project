import Synchronization

struct Pair {
  var a: Int
  var b: Int
}

func main() {
  // Mutex is used only through inlinable code, so the program doesn't link
  // libswiftSynchronization.
  let mutex = Mutex<Int>(15)
  mutex.withLock { $0 += 1 }
  let pair = Mutex<Pair>(Pair(a: 1, b: 2))
  pair.withLock { $0.b += 1 }
  print("break here")
}

main()
