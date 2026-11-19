protocol P {}

protocol PP<T> {
    associatedtype T
}

class TheBox<X>: P {
    var payload: X
    init(_ p: X) { self.payload = p }
}

class ParamBox<X>: P, PP {
    typealias T = X
    var payload: X
    init(_ p: X) { self.payload = p }
}

func entry<U>(_ value: U) {
    let variable: (TheBox<U> & P)? = TheBox(value)
    let paramOnly: (any PP<U> & P)? = ParamBox(value)
    let classBoundParam: (ParamBox<U> & PP<U> & P)? = ParamBox(value)
    print("break here")
}

entry(42)
