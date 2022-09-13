public struct GenericSBList<ElementType> : RandomAccessCollection {
    let getElement: (Int) -> ElementType
    public var startIndex: Int
    public var endIndex: Int
    
    public init(count: Int, getElement: @escaping (Int) -> ElementType) {
        startIndex = 0
        endIndex = count
        self.getElement = getElement
    }

    public init(slice: Range<Int>, of other: Self) {
        startIndex = slice.lowerBound
        endIndex = slice.upperBound
        getElement = other.getElement
    }
    
    @inline(__always) public func formIndex(after i: inout Int) { i += 1 }
    @inline(__always) public func formIndex(before i: inout Int) { i -= 1 }
    @inline(__always) public func index(i: Int, offsetBy: Int) -> Int { return i+offsetBy }

    @inline(__always) public subscript(i: Int) -> ElementType { getElement(i) }
    @inline(__always) public subscript(range: Range<Int>) -> Self { Self(slice: range, of: self) }
}
