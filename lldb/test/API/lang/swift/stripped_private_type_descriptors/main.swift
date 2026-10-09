import Lib

func use(_ msg: Message) {
  print(msg) // break here
}

func useCyclic(_ msg: CyclicMessage) {
  print(msg) // break cyclic
}

use(makeMessage())
useCyclic(makeCyclicMessage())
