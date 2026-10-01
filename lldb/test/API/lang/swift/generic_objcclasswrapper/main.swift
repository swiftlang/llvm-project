import Foundation

func f<T>(_ x : T) -> T {
  return x // break 2
}

let foo = NSError(domain: "patatino", code: 0, userInfo: [:]) // break 1
print(f(foo))
