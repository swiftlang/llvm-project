// main.swift
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2019 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
// -----------------------------------------------------------------------------
func stop() {}

class BlubbyUbby<T>
{
  var my_int : Int
  var my_string : String
  var my_t : T
  
  init(_ in_int: Int, _ in_string : String, _ in_t : T) {
    my_int = in_int
    my_string = in_string
    my_t = in_t
    stop()
    stop() // break 1
  }
}

var _ = BlubbyUbby<Int>(1, "some string", 0xDeadBeef)

struct S<T> {
  var a : T
  func foo() {
    stop()
    stop() // break 2
  }
}

func test<T>(_ t : T) {
  let a = S(a: t)
  a.foo()
}

test(12)
