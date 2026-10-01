class DefaultMirror {
  var a = "y12"
  var b = "q24"
}

class CustomMirror : CustomReflectable {
  var a = 12
  var b = 24
  
  public var customMirror: Mirror {
    get { return Mirror(self, children: ["c" : "t\(a + b)"]) }
  }
}

class CustomSummary : CustomStringConvertible, CustomDebugStringConvertible {
  var a = 12
  var b = 24
  
  var description: String { return "CustomStringConvertible" }
  var debugDescription: String { return "CustomDebugStringConvertible" }
}

class TheBase : CustomReflectable {
  var a = "y12"
  var b = "q24"
  
  public var customMirror: Mirror {
    get { return Mirror(self, children: ["a" : a, b : "b"], displayStyle: .`class`) }
  }
}

class TheDescendant : TheBase {
  var c = "t36"
}

class TheReflectiveDescendant: TheBase {
  var d = "w48"
  
  public override var customMirror: Mirror {
    get { return Mirror(self, children: ["d" : d], displayStyle: .`class`) }
  }
}

func main() {
  var dm = DefaultMirror()
  var cm = CustomMirror()
  var cs = CustomSummary()
  var td = TheDescendant()
  var tr = TheReflectiveDescendant()
  print("break here")
}

main()
