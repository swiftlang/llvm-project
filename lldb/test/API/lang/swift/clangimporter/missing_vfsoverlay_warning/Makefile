SWIFT_SOURCES := main.swift
# The test creates the overlay before the build and deletes it afterwards, so
# that only the path serialized into the debug info refers to it.
SWIFTFLAGS_EXTRAS = -Xcc -ivfsoverlay -Xcc $(BUILDDIR)/overlay.yaml

include Makefile.rules
