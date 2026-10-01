// Make sure [Any.Type] is reconstructed correctly.
// (and a subclass) conforming to Error correctly (and that we don't crash).
// This involves resolving the dynamic type correctly in the language runtime.

func main() -> Int {
  let patatino: [Any.Type] = [
    String.self,
    Int.self,
    Float.self,
  ]
  return 0 // break here
}

let _ = main()
