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

protocol MyProtocol {
  func bar();
}

struct StructTest {
  func foo() {
    print("break 1")
  }
}

extension StructTest : MyProtocol {
  func bar() {
    print("Stop here in MyProtocol method")
  }
}

class ClassTest {
  func foo () {
    print("break 2")
  }
}

var st = StructTest()
st.foo()
st.bar()

var ct = ClassTest()
ct.foo()
