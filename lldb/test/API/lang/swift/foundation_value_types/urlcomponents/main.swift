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

func main() {
  var urlc = URLComponents(string: "https://www.apple.com:12345/thisurl/isnotreal/itoldyou.php?page=fake")!
  print(urlc.scheme) // break 1
  print(urlc.host) // break 2
  print(urlc.port) // break 3
  print(urlc.path) // break 4
  print(urlc.query) // break 5
  print("break 6")
}

main()
