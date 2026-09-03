@main enum Entry {
    static func main() {
        var x = UniqueBox(23)
        print(x.value) // break here
        x.value = 41
        print(x.value) // break here
    }
}
