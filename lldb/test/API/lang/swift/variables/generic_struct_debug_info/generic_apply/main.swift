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
func apply<Type>(_ T : Type, fn: (Type) -> Type) -> Type { return fn(T) }

public func f<Type>(_ value : Type)
{
  apply(value) { arg in
    return arg // break here
  }
}

f(0xdeadbeef)
