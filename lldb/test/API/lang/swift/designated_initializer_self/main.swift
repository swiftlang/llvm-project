class C {
  init() {}
  convenience init(unused: Bool) { 
    self.init()
    print(1) // break here
  }
}

C(unused: true)

