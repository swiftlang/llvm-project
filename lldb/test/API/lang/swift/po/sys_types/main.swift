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
#if os(iOS)
    import UIKit
#elseif os(OSX)
    import AppKit
#endif    

func main() {
  var num = 22
  var str = "Hello world" // break 1
  var arr = [1,2,3,4] 
  var nsarr = NSMutableArray(array: arr) // break 2
#if os(iOS)
  var clr = UIColor.red // break 3
#elseif os(OSX)
  var clr = NSColor.red // break 3
#endif
  var nsobject = NSObject() // break 4
  var any: Any = 1234 // break 5
  var anyobject: AnyObject = 1234 as NSNumber // break 6
  var notification = Notification(name: Notification.Name(rawValue: "JustANotification"), object: nil)
  var lines = "one\ndue" // break 7
  print("break 8")
}

main()
