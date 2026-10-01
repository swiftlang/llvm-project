public enum Enum1 {
    case A
    case B
}
public enum Enum2 {
    case C
    case D
}
public enum SuperEnum {
    case Case1(Enum1)
    case Case2(Enum2)
}
let x = SuperEnum.Case1(.A)
let y = SuperEnum.Case1(.B)
let w = SuperEnum.Case2(.C)
let z = SuperEnum.Case2(.D)
print() // break here
