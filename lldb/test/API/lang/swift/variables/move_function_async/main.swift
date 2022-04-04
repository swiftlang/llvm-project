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

public class Klass {
    public func doSomething() {}
}

public func forceSplit() async {}

//////////////////
// Simple Tests //
//////////////////

public func copyableValueTest() async {
    print("stop here") // Set breakpoint copyableValueTest here 1
    let k = Klass()
    k.doSomething()
    await forceSplit() // Set breakpoint copyableValueTest here 2
    let m = _move(k) // Set breakpoint copyableValueTest here 3
    m.doSomething() // Set breakpoint copyableValueTest here 4
    await forceSplit()
    m.doSomething() // Set breakpoint copyableValueTest here 5
}

//////////////////////////
// Top Level Entrypoint //
//////////////////////////

@main struct Main {
  static func main() async {
    await copyableValueTest()
  }
}
