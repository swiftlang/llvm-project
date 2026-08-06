protocol P1 {
    func foo() -> String
}

protocol P2 {
    func bar() -> String
}

class C: P1, P2 {
    let i = 42

    func foo() -> String {
        "In class foo"
    }

    func bar() -> String  {
        "In class bar"
    }
}

struct S: P1, P2 {
    let str = "Hello"

    func foo() -> String {
        "In struct foo"
    }

    func bar() -> String {
        "In struct bar"
    }
}

func f() {
    let c: any P1 & P2 = C()
    let s: any P1 & P2 = S()
    // Use func so they aren't optimized out in embedded swift
    print(c.foo())
    print(c.bar())
    print(s.foo())
    print(s.bar())
    print("break here")
}

f()

