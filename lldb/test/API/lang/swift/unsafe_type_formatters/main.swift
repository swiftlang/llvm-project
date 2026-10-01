struct IntPair {
  var original: Int
  var opposite: Int

  init(_ value: Int) {
    self.original = value
    self.opposite = -value
  }
}

enum Toggle { case On; case Off }

enum ColorCode {
  case RGB(UInt8, UInt8, UInt8)
  case Hex(Int)
}

protocol Flyable {
  var fly : String { get }
}

struct Bird: Flyable {
  var fly: String = "🦅"
}

struct Plane: Flyable {
  var fly: String = "🛩"
}

class Number<T:Numeric> {
  var number_value : T

  init (number value : T) {
    number_value = value
  }
}

// Put "Raw" in the name to test that the data formatter is not confused into
// choosing a one of the Unsafe*Raw types.
struct NotRaw {
  var x: Int
}

func main() {
  // UnsafeBufferPointer
  let structArray = [ IntPair(1), IntPair(-2), IntPair(3) ]
  structArray.withUnsafeBufferPointer {
    let buf = $0
    print("break here ...")
  } // break 01

  // UnsafeMutableBufferPointer
  var enumArray = [ Toggle.Off ]
  enumArray.withUnsafeMutableBufferPointer {
    let mutbuf = $0
    print("... here ...")
    mutbuf[0] = Toggle.On // break 02
    print("... and here!")
  } // break 03

  var colors = [ColorCode.RGB(155,219,255), ColorCode.Hex(0x4545ff)]

  let unsafe_ptr = UnsafePointer(&colors[0])

  var unsafe_mutable_ptr = UnsafeMutablePointer(&colors[1]) // break 04

  let unsafe_raw_ptr = UnsafeRawPointer(&colors[0]) // break 05

  colors.withUnsafeBufferPointer { // break 06
    let buf = $0
    print("break")
  } // break 07

  var flyingObjects : [Flyable] = [ Bird(), Plane() ]

  flyingObjects.withUnsafeMutableBufferPointer {
    let mutbuf = $0
    struct UFO: Flyable {
      var fly: String = "🛸"
    }

    mutbuf[1] = UFO()
  } // break 08

  let numbers = [ Number(number: 42), Number(number: 3.14)]

  numbers.withUnsafeBufferPointer {
    let buf = $0
    print("break")
  } // break 09

  // UnsafeRawBufferPointer
  let bytes = [UInt8](0...255)

  bytes.withUnsafeBufferPointer {
    let buf = $0
    let rawbuf = UnsafeRawBufferPointer(buf)
    print("break")
    typealias ByteBuffer = UnsafeRawBufferPointer;
    let alias = rawbuf as ByteBuffer
    print("break 10")
    typealias ByteBufferAlias = ByteBuffer
    let secondAlias = alias as ByteBufferAlias
    print("break 11")
  } // break 12

  // UnsafeMutableRawBufferPointer
  var bits : [UInt8] = [0,1]

  bits.withUnsafeMutableBufferPointer {
    var mutbuf = $0

    let mutrawbuf = UnsafeMutableRawBufferPointer(mutbuf)

    mutrawbuf.swapAt(0, 1) // break 13
  } // break 14

  let cooked: [NotRaw] = [.init(x: 1), .init(x: 2), .init(x: 4)]
  cooked.withUnsafeBufferPointer { buffer in
    print("break")
  } // break 15
}

main()
