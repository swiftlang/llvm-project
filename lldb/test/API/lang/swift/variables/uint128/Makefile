SWIFT_SOURCES := main.swift
# Int128 and UInt128 carry a stdlib availability annotation newer than the
# deployment target the test suite compiles against.
SWIFTFLAGS_EXTRAS := -Xfrontend -disable-availability-checking

include Makefile.rules
