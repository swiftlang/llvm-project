public class PublicBase {}

private class PrivateSubclass: PublicBase, CustomStringConvertible {
  var description: String
  init(description: String) {
    self.description = description
  }
}

private class PrivateClass: CustomStringConvertible {
  var description = "Meat pie"
}

func makePublic() -> PublicBase {
  return PrivateSubclass(description: "Easy as pie")
}

func makeAnyObject() -> AnyObject {
  return PrivateSubclass(description: "Any pie")
}

@main enum Entry {
  static func main() {
    let x = makePublic()
    let y = makeAnyObject()
    let z = PrivateClass()
    print("break here", x, y, z)
  }
}
