protocol P {}

struct S : P {
    var type = "pata"
    var stringValue = "tino"
}

let tinky : P = S()
print() // break here
