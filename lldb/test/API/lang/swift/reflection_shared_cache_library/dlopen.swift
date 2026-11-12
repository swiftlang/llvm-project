import Darwin
import Synchronization

func main() {
  // Mutex is used only through inlinable code, so the program doesn't link
  // libswiftSynchronization until it loads it explicitly.
  let mutex = Mutex<Int>(15)
  mutex.withLock { $0 += 1 }
  print("before load")
  let handle = dlopen("/usr/lib/swift/libswiftSynchronization.dylib", RTLD_NOW)
  print("after load", handle != nil)
}

main()
