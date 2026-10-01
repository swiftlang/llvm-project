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
import Foundation

class Test: NSArray { 
    override var count: Int { return 1 } 
    override func object(at index: Int) -> Any { return "abc" } 
    override func copy(with: NSZone?) -> Any { return self } 
}

func main() {
  var t = Test()
  var ta = Test() as Array
  var tb = Test() as Array + []

  print("break here")
}

main()
