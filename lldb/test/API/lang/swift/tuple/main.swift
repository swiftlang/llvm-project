// main.swift
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2016 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
// -----------------------------------------------------------------------------
struct HasTuple {
  let tup: (Int, () -> Int) = (123, { 321 })
}

func main() {
  let s = HasTuple()
  let any_inlined_tuple: Any = (1, 2)
  let any_boxed_tuple: Any = ("tuple", 42, [1, 2, 3])
  print("break here")
}

main()
