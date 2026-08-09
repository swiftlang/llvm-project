// A type declared inside an extension has an `Extension` node in the decl
// context of its mangled name whenever the mangler cannot elide the extension:
// when the extension is constrained, when it is on a protocol, or when it is in
// a different module than the type it extends. Everything here is in a single
// module, so the constrained extensions are what produce the `Extension` node.

struct Box<T> {
  var t: T
}

extension Box {
  // Unconstrained and in the same module: the mangler elides the extension
  // entirely, so this type's decl context is indistinguishable from direct
  // nesting. Control case.
  struct InPlainExt {
    var v: Int
  }
}

extension Box where T == Int {
  // Constrained, so mangled as
  // Structure(Extension(main, BoundGenericStructure, GenericSignature), ...).
  struct InConstrExt {
    var v: Int
  }

  // A private type in the same extension. Its private discriminator becomes a
  // DW_TAG_namespace between the extended nominal and the type itself.
  private struct Hidden {
    var w: Int
  }

  struct Holder {
    private var hidden = Hidden(w: 30)
    var tag = 31
    init() {}
  }
}

final class Ref<T> {
  var t: T
  init(t: T) { self.t = t }
}

extension Ref where T == Int {
  // The extended type of a constrained extension of a generic class demangles
  // to a BoundGenericClass rather than a BoundGenericStructure.
  struct InClassConstrExt {
    var v: Int
  }
}

func main() {
  let plain = Box<Int>.InPlainExt(v: 10)
  let constr = Box<Int>.InConstrExt(v: 20)
  let holder = Box<Int>.Holder()
  let cls = Ref<Int>.InClassConstrExt(v: 40)
  print("break here")
  print(plain.v)
  print(constr.v)
  print(holder.tag)
  print(cls.v)
}

main()
