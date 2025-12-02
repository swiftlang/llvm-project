struct OpaqueError: Error {}

func untyped() throws {
    throw OpaqueError()
}

func typed() throws(OpaqueError) {
    throw OpaqueError()
}

@main struct Entry {
    static func main() {
        do {
            try untyped()
        } catch {}
        do {
            try typed()
        } catch {}
    }
}
