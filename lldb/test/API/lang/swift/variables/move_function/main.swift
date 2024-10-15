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

public protocol P {
    static var value: P { get }
    func doSomething()
}

extension Klass : P {
    public static var value: P { Klass() }
}

var trueBoolValue : Bool { true }
var falseBoolValue : Bool { false }

//////////////////
// Simple Tests //
//////////////////

public func copyableValueTest() {
    print("stop here") // Set breakpoint copyableValueTest here 1
    let k = Klass()
    k.doSomething()
    let m = _move(k) // Set breakpoint copyableValueTest here 2
    m.doSomething() // Set breakpoint copyableValueTest here 3
}

public func copyableVarTest() {
    print("stop here") // Set breakpoint copyableVarTest here 1
    var k = Klass()
    k.doSomething()
    let m = _move(k) // Set breakpoint copyableVarTest here 2
    m.doSomething()
    k = Klass()     // Set breakpoint copyableVarTest here 3
    k.doSomething() // Set breakpoint copyableVarTest here 4
    print("stop here")
}

public func addressOnlyValueTest<T : P>(_ x: T) {
    print("stop here") // Set breakpoint addressOnlyValueTest here 1
    let k = x
    k.doSomething()
    let m = _move(k) // Set breakpoint addressOnlyValueTest here 2
    m.doSomething() // Set breakpoint addressOnlyValueTest here 3
}

public func addressOnlyVarTest<T : P>(_ x: T) {
    print("stop here") // Set breakpoint addressOnlyVarTest here 1
    var k = x
    k.doSomething()
    let m = _move(k) // Set breakpoint addressOnlyVarTest here 2
    m.doSomething()
    k = x // Set breakpoint addressOnlyVarTest here 3
    k.doSomething() // Set breakpoint addressOnlyVarTest here 4
}

////////////////////////////////////
// Conditional Control Flow Tests //
////////////////////////////////////

public func copyableValueCCFTrueTest() {
    let k = Klass() // Set breakpoint copyableValueCCFTrueTest here 1
    k.doSomething() // Set breakpoint copyableValueCCFTrueTest here 2
    if trueBoolValue {
        let m = _move(k) // Set breakpoint copyableValueCCFTrueTest here 3
        m.doSomething() // Set breakpoint copyableValueCCFTrueTest here 4
    }
    // Set breakpoint copyableValueCCFTrueTest here 5
}

public func copyableValueCCFFalseTest() {
    let k = Klass() // Set breakpoint copyableValueCCFFalseTest here 1
    k.doSomething() // Set breakpoint copyableValueCCFFalseTest here 2
    if falseBoolValue {
        let m = _move(k)
        m.doSomething()
    }
    // Set breakpoint copyableValueCCFFalseTest here 3
}

//////////////////////////
// Top Level Entrypoint //
//////////////////////////

func main() {
    copyableValueTest()
    copyableVarTest()
    addressOnlyValueTest(Klass())
    addressOnlyVarTest(Klass())
    copyableValueCCFTrueTest()
    copyableValueCCFFalseTest()
}

main()
