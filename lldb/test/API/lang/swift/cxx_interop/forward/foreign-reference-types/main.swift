import FRT

struct StructWithFRTs {
  let shared: SharedFRT
  let maybeShared: SharedFRT?
  let immortal: ImmortalFRT
  let virtualRC: VirtualRefCounted
  let number = 42
}

final class ClassWithFRTs {
  let shared: SharedFRT
  let maybeShared: SharedFRT?
  let immortal: ImmortalFRT
  let number = 43

  init(shared: SharedFRT, immortal: ImmortalFRT) {
    self.shared = shared
    self.maybeShared = shared
    self.immortal = immortal
  }
}

func use<T>(_ t: T) {}

func generic<T>(_ t: T) {
  print("break in generic")
}

func main() {
  let shared = SharedFRT.create(1)
  let immortal = ImmortalFRT.get()
  let unsafe = UnsafeFRT.create()
  let maybeShared: SharedFRT? = SharedFRT.create(4)
  let noShared: SharedFRT? = nil
  var reassigned: SharedFRT? = nil
  reassigned = shared

  let member = MemberRefCounted(6)
  let virtualRC = VirtualRefCounted(7)
  let derived = DerivedVirtualRefCounted(8, 9)
  let upcast: VirtualRefCounted = derived

  let nonPrimary = NonPrimaryFRT.create(11)
  let nonPrimaryBase = asSharedFRT(nonPrimary)

  let incomplete = createIncompleteFRT(12)
  let opaque = createOpaqueFRT()
  let optionalOnly = OptionalOnlyFRT.create(16)
  let holder = makeHoldsFRT(shared)

  let structWithFRTs = StructWithFRTs(
    shared: shared, maybeShared: shared, immortal: immortal,
    virtualRC: derived)
  let classWithFRTs = ClassWithFRTs(shared: shared, immortal: immortal)

  print("break here")
  use((shared, immortal, unsafe, maybeShared, noShared, reassigned, member,
       virtualRC, derived, upcast, nonPrimary, nonPrimaryBase, incomplete,
       opaque, optionalOnly, holder, structWithFRTs, classWithFRTs))
  generic(shared)
}

main()
