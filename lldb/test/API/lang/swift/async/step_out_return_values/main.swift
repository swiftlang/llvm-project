struct E: Error {}

struct LargeE: Error {
  var a = 1
  var b = 2
  var c = 3
  var d = 4
  var e = 5
}

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

func f_typed_throws() async throws(E) -> Int {
  let v_throws = try! await f_throws()
  return 17
}

func f_typed_throws_large() async throws(LargeE) -> Int {
  let v_typed_throws = try! await f_typed_throws()
  return 18
}

func f_throwing() async throws -> Int {
  let y = await f_int(0)
  if y == 1 { throw E() } // break in throwing
  return y
}

func run() async {
  let v_typed_throws_large = try! await f_typed_throws_large()
  _ = v_typed_throws_large
  _ = try? await f_throwing()
}

await run()
