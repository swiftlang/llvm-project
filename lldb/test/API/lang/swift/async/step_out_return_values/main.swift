struct TwoInt {
  var a = 3
  var b = 4
}

func f_int(_ x: Int) async -> Int {
  return x + 1 // break here
}

func f_string() async -> String {
  let v_int = await f_int(1)
  return "hello"
}

func f_two_int() async -> TwoInt {
  let v_string = await f_string()
  return TwoInt()
}

func f_throws() async throws -> Int {
  let v_two_int = await f_two_int()
  return 14
}

func run() async {
  let v_throws = try! await f_throws()
  _ = v_throws
}

await run()
