class C: CustomStringConvertible {
    var description = "instance of C"

    class func doSomething() {
        let value = C()
        print("break here", value)
    }
}

@main enum Entry {
    static func main() {
        C.doSomething()
    }
}
