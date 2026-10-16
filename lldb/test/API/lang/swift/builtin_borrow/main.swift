// Small bitwise-borrowable values are borrowed as an inline copy, so the
// borrow has the referent's size and layout. Large values are borrowed by
// pointer.
struct Small {
  var flag = true
  var count: Int16 = 7
}

struct Large {
  var a = 1, b = 2, c = 3, d = 4, e = 5
}

// The Refs are formed in the caller so that by-pointer borrows refer to the
// caller's variables.
struct Holder: ~Escapable {
  let byte: Ref<UInt8>
  let small: Ref<Small>
  let int: Ref<Int>
  let large: Ref<Large>

  @_lifetime(copy byte, copy small, copy int, copy large)
  init(
    _ byte: Ref<UInt8>, _ small: Ref<Small>, _ int: Ref<Int>,
    _ large: Ref<Large>
  ) {
    self.byte = byte
    self.small = small
    self.int = int
    self.large = large
  }
}

// String is addressable for dependencies, so it's borrowed by pointer even
// though it's small enough to be borrowed inline.
struct StringHolder: ~Escapable {
  let string: Ref<String>
  let int: Ref<Int>

  @_lifetime(copy string, copy int)
  init(_ string: Ref<String>, _ int: Ref<Int>) {
    self.string = string
    self.int = int
  }
}

func use(_ holder: borrowing Holder) {}
func use(_ holder: borrowing StringHolder) {}
func use<T>(_ ref: Ref<T>) {}

// Five floats fit in four words but are passed indirectly, and four vectors
// are larger than four words but are passed directly, so the representation of
// borrows of these types depends on whether it's chosen by size or by calling
// convention.
struct Floats5 {
  var a: Float = 1, b: Float = 2, c: Float = 3, d: Float = 4, e: Float = 5
}

struct Vecs4 { var a, b, c, d: SIMD4<Float> }

struct MismatchHolder: ~Escapable {
  let floats: Ref<Floats5>
  let int: Ref<Int>

  @_lifetime(copy floats, copy int)
  init(_ floats: Ref<Floats5>, _ int: Ref<Int>) {
    self.floats = floats
    self.int = int
  }
}

func use(_ holder: borrowing MismatchHolder) {}
func use(_ values: Int...) {}

// Unspecialized, so the size comes from the runtime's type metadata.
func runtimeRefSize<T>(_: T.Type) -> Int { MemoryLayout<Ref<T>>.size }

func generic<T>(_ value: borrowing T) {
  let ref = Ref(value)
  print("break in generic")
  use(ref)
}

func test() {
  let byte: UInt8 = 3
  let small = Small()
  let int = 42
  let large = Large()
  let holder = Holder(Ref(byte), Ref(small), Ref(int), Ref(large))
  let string = "hello"
  let stringHolder = StringHolder(Ref(string), Ref(int))
  let floats = Floats5()
  let mismatchHolder = MismatchHolder(Ref(floats), Ref(int))
  let mismatchHolderSize = MemoryLayout<MismatchHolder>.size
  let floatsStaticRefSize = MemoryLayout<Ref<Floats5>>.size
  let floatsRuntimeRefSize = runtimeRefSize(Floats5.self)
  let vectorsStaticRefSize = MemoryLayout<Ref<Vecs4>>.size
  let vectorsRuntimeRefSize = runtimeRefSize(Vecs4.self)
  print("break here")
  use(holder)
  use(stringHolder)
  use(mismatchHolder)
  use(mismatchHolderSize, floatsStaticRefSize, floatsRuntimeRefSize,
      vectorsStaticRefSize, vectorsRuntimeRefSize)
  generic(byte)
  generic(small)
  generic(large)
}

test()
