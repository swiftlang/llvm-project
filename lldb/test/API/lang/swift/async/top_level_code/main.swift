func getValue() async -> Int {
  return 42
}

let value = await getValue()
print(value) // break here
