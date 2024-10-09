@_originallyDefinedIn(
     module: "Other", iOS 2.0, macOS 2.0, tvOS 2.0, watchOS 2.0)
@available(iOS 1.0, macOS 1.0, tvOS 1.0, watchOS 1.0, *)
public struct A {
    let i = 10
}

@_originallyDefinedIn(
     module: "Other", iOS 2.0, macOS 2.0, tvOS 2.0, watchOS 2.0)
@available(iOS 1.0, macOS 1.0, tvOS 1.0, watchOS 1.0, *)
public struct B {
    let i = 20
}

typealias Alias = B

@_originallyDefinedIn(
     module: "Other", iOS 2.0, macOS 2.0, tvOS 2.0, watchOS 2.0)
@available(iOS 1.0, macOS 1.0, tvOS 1.0, watchOS 1.0, *)
public enum C {
    public struct D {
        let i = 30
    }
}

@_originallyDefinedIn(
     module: "Other", iOS 2.0, macOS 2.0, tvOS 2.0, watchOS 2.0)
@available(iOS 1.0, macOS 1.0, tvOS 1.0, watchOS 1.0, *)
public enum E<T> {
    case t(T)
}

public struct Prop {
    let i = 40
}

func f() {
    let a = A()
    let b = Alias()
    let d = C.D()
    let e = E<Prop>.t(Prop())
    print("break here")
}

f()


