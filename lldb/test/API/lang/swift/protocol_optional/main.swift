protocol Key {
    associatedtype Value
}

struct Key1: Key {
    typealias Value = Int?
}

struct KeyTransformer<K1: Key> {
    let input: K1.Value

    func printOutput() {
        let patatino = input
        print(patatino) // break here
    }
}

var xformer: KeyTransformer<Key1> = KeyTransformer(input: 5)
xformer.printOutput()
