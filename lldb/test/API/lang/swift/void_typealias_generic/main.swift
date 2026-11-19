public struct Box<T> {
    public let payload: T
    public let tag: Int
}

func f() {
    let boxOfVoid = Box<Void>(payload: (), tag: 7)
    let nestedVoid: (Void, Int) = ((), 42)

    print("break here")
}

f()
