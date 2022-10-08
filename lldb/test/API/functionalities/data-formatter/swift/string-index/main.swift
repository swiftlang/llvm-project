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
    exerciseNative()
    exerciseBridged()
}

func exerciseNative() {
    exercise("a👉🏼b")
}

func exerciseBridged() {
    exercise("a👉🏼b" as NSString as String)
}

func exercise(_ string: String) {
    let nativeIndices = allIndices(string)
    let unicodeScalarIndices = allIndices(string.unicodeScalars)
    let utf8Indicies = allIndices(string.utf8)
    let utf16Indicies = allIndices(string.utf16)
    // break here
}

func allIndices<T: Collection>(_ collection: T) -> [T.Index] {
    return Array(collection.indices) + [collection.endIndex]
}

main()
