func f() {
    let mostNegative = Int128.min
    let mostPositive = Int128.max
    let negative: Int128 = -42
    let positive: Int128 = 42

    print("break here")
    _ = (mostNegative, mostPositive, negative, positive)
}

f()
