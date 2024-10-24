@_DebugDescription
struct Unit {
  var description: String { "fulfilled" }
}

func main() {
  let unit = Unit()
  print("break here", unit)
}

main()
