// A function reference is a value whose storage is the address of a global
// rather than the result of an instruction.

func target(_ x: Int) -> Int { return x + 1 }

func f() {
  let cfn: @convention(c) (Int) -> Int = target
  let thinfn: @convention(thin) (Int) -> Int = target
  print(cfn(1)) // break here
  print(thinfn(2))
}

f()
