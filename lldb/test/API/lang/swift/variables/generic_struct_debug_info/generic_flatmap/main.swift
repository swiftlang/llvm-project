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
struct Product
{
    let name : String
    let ID : String
}

struct ValueAndIndices<Type>
{
    let originalIndex : Int
    let filteredIndex : Int
    let value : Type
}


struct FlatMapper<Type>
{
    init(values : [Type])
    {
        precondition(values.count == 1)
        
        let tuples = [
            ValueAndIndices(originalIndex: 0, filteredIndex: 0, value: values[0]),
        ]
        
        let _ = tuples.flatMap { tuple in
            return tuple // break 1
        }
        
       let _ = values.flatMap { value in
            return value // break 2
        }
    }
}

let values = [
    Product(name: "Coffee", ID: "1"),
]

print(FlatMapper(values: values))
