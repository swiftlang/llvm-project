func use(_ fn: (Int) -> Int) {
  print(fn(1)) // break here
}

var captured = 42
use { $0 + captured }
