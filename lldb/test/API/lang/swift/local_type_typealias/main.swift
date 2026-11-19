import Foundation

func f(input: MyAliasedKey) {
  class LocalBox { var slot = 7 }
  let variable = LocalBox()
  print("break here")
  _ = variable.slot
}

f(input: MyAliasedKey(""))
