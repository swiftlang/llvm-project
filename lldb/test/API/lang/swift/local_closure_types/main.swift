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
let foo: () -> () = {
  struct S {
    let i: Int
  }
  let s = S(i: 777)
  print(s.i) // break here
}

// Two closures in the same function declaring types with the same name.
func twoClosures() {
  let first = {
    struct S {
      let i: Int
    }
    let s = S(i: 1)
    print(s.i) // break in first closure
  }
  let second = {
    struct S {
      let a: Int
      let b: Int
    }
    let s = S(a: 2, b: 3)
    print(s.b) // break in second closure
  }
  first()
  second()
}

func nestedClosures() {
  let outer = {
    let inner = {
      struct T {
        let k: Int
      }
      let t = T(k: 4)
      print(t.k) // break in nested closure
    }
    inner()
  }
  outer()
}

foo()
twoClosures()
nestedClosures()
