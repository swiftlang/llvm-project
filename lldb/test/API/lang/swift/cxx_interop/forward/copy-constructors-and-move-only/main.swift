import Types

struct SwiftStructWithCopyCounted {
  let member: CopyCounted
  let number = 42
}

struct SwiftStructWithNonCopyable: ~Copyable {
  let member: NonCopyable
  let number = 43
}

final class SwiftClassWithNonCopyable {
  let member: NonCopyable
  let number = 44

  init(_ x: CInt) { member = NonCopyable(x) }
}

func use<T: ~Copyable>(_ t: borrowing T) {}

func main() {
  let copyCounted = CopyCounted(1)
  let hasMember = HasCopyCountedMember(2, 3)
  let virtualBase = HasVirtualBase()
  let swiftStruct = SwiftStructWithCopyCounted(member: CopyCounted(4))

  let nonCopyable = NonCopyable(5)
  let holder = NonCopyableHolder(6)
  let derived = NonCopyableDerived(7)
  let annotated = AnnotatedNonCopyable(x: 8)
  let swiftNC = SwiftStructWithNonCopyable(member: NonCopyable(9))
  let classNC = SwiftClassWithNonCopyable(10)

  print("break here")
  use(copyCounted)
  use(hasMember)
  use(virtualBase)
  use(swiftStruct)
  use(nonCopyable)
  use(holder)
  use(derived)
  use(annotated)
  use(swiftNC)
  use(classNC)
}

main()
