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
func main() -> Int {

    struct Foo {
        init () {
            a = 234
            b = 1.25
        }
        var a : Int;
        var b : Double;

        struct Bar {
            init () {
                c = 48
                d = "Hello"
            }
            var c: Int8;
            var d: String;
        }
    }
    
    class Base {
      var a = 1
    }
    
    class Derived : Base {
      var b = 2
    }

    var f = Foo()
    var b = Foo.Bar()
    var c = Derived()
    print("break 1")
    print("break 2")
    print("break 3")
    print("break 4")
    print("break 5")
    print("break 6")
    return 0
}

main()
