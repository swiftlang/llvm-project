enum MyErr : Error {
  case Patatino
  case Mio
}

func f<T>(_ Pat : T) -> T {
  return Pat // break here
}

f(MyErr.Patatino as Error)
