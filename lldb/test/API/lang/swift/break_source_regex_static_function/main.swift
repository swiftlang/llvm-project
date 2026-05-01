enum Scope {
    static func first() {
        _ = "break here"
    }

    static func second() {
        _ = "break here"
    }
}

@main enum Entry {
    static func main() {
        Scope.first()
        Scope.second()
    }
}
