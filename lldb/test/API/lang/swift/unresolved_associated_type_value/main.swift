protocol Printable {
  func describe() -> String
}

protocol Container {
  associatedtype Element
  var items: [Element] { get }
}

extension Container where Element: Printable {
  func printAll() -> String {
    return items.map {
      $0.describe() // break here
    }.joined(separator: ", ")
  }
}

class SpecificClass: Printable {
  var name: String
  init(name: String) { self.name = name }
  func describe() -> String { return "\(name)" }
}

struct Stack: Container {
  var items: [SpecificClass] = []
}

let stack = Stack(items: [SpecificClass(name: "Rex")])
print(stack.printAll())
