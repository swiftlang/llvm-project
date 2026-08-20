func sayolleH(str : String) async {
  var str = "in sayolleH"
  print("\(str) before calls") //break here
}

func sayHello() async {
  var str = "in hello"
  await sayolleH(str:"hello")
}

func sayGeneric<T>(_ msg: T) async {
  var str = "in generic"
  await sayHello()
}

func synchronousSayHelo() {
  print("synchronously saying hello") // break synchronous hello
}

func callSyncHello() async {
  synchronousSayHelo() // frame 1 line
}

@main struct Main {
  static func main() async {
    await sayGeneric("world")
    await sayHello()
    await callSyncHello()
  }
}
