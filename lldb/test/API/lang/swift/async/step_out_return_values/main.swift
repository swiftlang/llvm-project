struct E: Error {}

struct LargeE: Error {
  var a = 1
  var b = 2
  var c = 3
  var d = 4
  var e = 5
}

class K {
  var a = 1
  var b = 2
}

protocol P {}
extension Int: P {}

struct TwoInt {
  var a = 3
  var b = 4
}

struct TwoDouble {
  var a = 1.5
  var b = 2.5
}

struct TwoFloat {
  var a: Float = 1.5
  var b: Float = 2.5
}

struct WrappedDouble {
  var value = 3.5
}

struct FloatInt {
  var f: Float = 1.5
  var i = 7
}

struct ThreeInt {
  var a = 1
  var b = 2
  var c = 3
}

enum Payload { case number(Int), text(String) }

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

func f_two_double() async -> TwoDouble {
  return TwoDouble() // break in known problem
}
func f_two_float() async -> TwoFloat {
  return TwoFloat() // break in known problem
}
func f_wrapped_double() async -> WrappedDouble {
  return WrappedDouble() // break in known problem
}
func f_float_int() async -> FloatInt {
  return FloatInt() // break in known problem
}
func f_float_int_tuple() async -> (Float, Int) {
  return (3.5, 11) // break in known problem
}
func f_three_int() async -> ThreeInt {
  return ThreeInt() // break in known problem
}
func f_payload() async -> Payload {
  return .number(5) // break in known problem
}
func f_existential() async -> any P {
  return 13 // break in known problem
}
func f_generic<T>(_ t: T) async -> T {
  return t // break in known problem
}
func f_optional_int() async -> Int? {
  return 5 // break in known problem
}
func f_optional_nil() async -> Int? {
  return nil // break in known problem
}
func f_optional_class() async -> K? {
  return K() // break in known problem
}
func f_optional_string() async -> String? {
  return "optional" // break in known problem
}
func f_array() async -> [Int] {
  return [1, 2, 3] // break in known problem
}
func f_dictionary() async -> [Int: String] {
  return [1: "one"] // break in known problem
}
func f_typed_throwing() async throws(E) -> Int {
  throw E() // break in known problem
}
func f_typed_throwing_large() async throws(LargeE) -> Int {
  throw LargeE() // break in known problem
}

func run() async {
  let v_typed_throws_large = try! await f_typed_throws_large()
  _ = v_typed_throws_large
  _ = try? await f_throwing()
  let v_two_double = await f_two_double()
  let v_two_float = await f_two_float()
  let v_wrapped_double = await f_wrapped_double()
  let v_float_int = await f_float_int()
  let v_float_int_tuple = await f_float_int_tuple()
  let v_three_int = await f_three_int()
  let v_payload = await f_payload()
  let v_existential = await f_existential()
  let v_generic = await f_generic(15)
  let v_optional_int = await f_optional_int()
  let v_optional_nil = await f_optional_nil()
  let v_optional_class = await f_optional_class()
  let v_optional_string = await f_optional_string()
  let v_array = await f_array()
  let v_dictionary = await f_dictionary()
  do {
    _ = try await f_typed_throwing()
  } catch let v_typed_throwing {
    _ = v_typed_throwing
  }
  do {
    _ = try await f_typed_throwing_large()
  } catch let v_typed_throwing_large {
    _ = v_typed_throwing_large
  }
  _ = (v_two_double, v_two_float, v_wrapped_double, v_float_int,
       v_float_int_tuple, v_three_int, v_payload, v_existential, v_generic,
       v_optional_int, v_optional_nil, v_optional_class, v_optional_string,
       v_array, v_dictionary)
}

await run()
