// `big` has its high bit set, so it only renders correctly if the formatter
// treats a UInt128 as unsigned. The controls are a UInt128 with the high bit
// clear, and an Int128, which really is signed.

func f() {
    let big: UInt128 = 0xF11E_2D3C_4B5A_6978_8796_A5B4_C3D2_E1F0
    let small: UInt128 = 42
    let signed: Int128 = -42

    print("break here")
    _ = (big, small, signed)
}

f()
