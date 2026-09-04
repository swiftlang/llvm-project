import Lib

// An extension of Lib.Root compiled into this module: `self` is a type from
// another module, so LLDB has to go through Lib to make sense of it.
extension Root {
  var doubled: Int {
    let factor = 7
    return value * factor // break here
  }
}

print(Root(value: 21).doubled)
