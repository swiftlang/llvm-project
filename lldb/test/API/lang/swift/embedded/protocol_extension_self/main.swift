// Inside a class-bound protocol extension the static type of `self` is the
// generic archetype τ_0_0, so inspecting it at all requires binding that
// archetype to the concrete type the caller passed.
protocol P: AnyObject {
    func payload() -> Int
}

class C: P {
    let number = 42
    func payload() -> Int { number }
}

class D: P {
    let other = 99
    func payload() -> Int { other }
}

extension P {
    func useSelf() {
        print("break here")
    }
}

let c: P = C()
c.useSelf()
let d: P = D()
d.useSelf()
