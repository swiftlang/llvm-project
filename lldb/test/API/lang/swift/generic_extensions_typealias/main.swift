extension Array where Element: Comparable {
  public func union(_ rhs: [Element]) -> [Element] {
    return [] // break 1
  }
}

var patatino = [1]
patatino.union([2])

extension Collection where Element: Equatable {
    func split<C: Collection>(separatedBy separator: C) -> [SubSequence] where C.Element == Element {
        var results = [SubSequence]() // break 2
        return results
    }
}

"patatino".split(separatedBy: "p")
