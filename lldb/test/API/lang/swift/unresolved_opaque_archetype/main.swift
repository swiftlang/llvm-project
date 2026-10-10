import OpaqueLib

struct Box<T> { var value: T; var extra: Int }
enum Either<T> { case left(T); case right(Int) }
final class Ref<T> { var value: T; init(_ v: T) { value = v } }
struct Tup<each T> { var vals: (repeat each T) }
struct N<T> { var v: T }

func mkPack<each T>(_ v: repeat each T) -> Tup<repeat each T> {
  Tup(vals: (repeat each v))
}

func use() {
  let box = Box(value: makeOpaque(), extra: 42)
  let either = Either.left(makeOpaque())
  let ref = Ref(makeOpaque())
  let opt = Optional(makeOpaque())
  let arr = [makeOpaque()]
  let dict = [1: makeOpaque()]
  let pack = mkPack(makeOpaque(), 42)
  // The shape of SwiftUI's TupleView<(Text, ModifiedContent<some View, M>)>.
  let tuple = Box(value: (1, Box(value: makeOpaque(), extra: 2)), extra: 3)
  let nested = Box(value: Either.left(Optional(makeOpaque())), extra: 1)
  let two = N(v: N(v: makeOpaque()))
  let deep = N(v: N(v: N(v: N(v: N(v: N(v: makeOpaque()))))))
  print("break here")
  _ = (box, either, ref, opt, arr, dict, pack, tuple, nested, two, deep)
}

use()
