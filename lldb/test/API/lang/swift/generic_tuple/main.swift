// main.swift
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2018 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
// -----------------------------------------------------------------------------
func use<T>(_ t : T) {}

func single<T>(_ t : T) {
  let x = t
  use(x) // break 1
}

func string_tuple<T, U>(_ t : (T, U)) {
  let (_, y) = t
  use(y) // break 2
}

func int_tuple<T, U>(_ t : (T, U)) {
  let (_, y) = t
  use(y) // break 3
}

let s = "hello"
single(s)
string_tuple((s, s))
int_tuple((Int32(111), Int64(222)))
