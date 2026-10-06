// Make sure we print array of tuples containing elements with
// resilient types print correctly.

import Foundation

var patatino : [(Data, Int64)] = [(Data([1, 2, 3]), 1001)]
var tinky : [(Data, Data)] = [(Data([1, 2, 3]), Data([9]))]
print(patatino) // break 1

print(tinky) // break 2
