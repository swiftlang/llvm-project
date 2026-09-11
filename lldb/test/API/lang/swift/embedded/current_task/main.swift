import _Concurrency

@main struct Main {
    static func main() async {
        withUnsafeCurrentTask { task in
            if task != nil {
                print("break here") // break here
            }
        }
    }
}
