# Explicit module builds fail to find the libstdc++ module map.
ifeq "$(OS)" "Linux"
SWIFT_ENABLE_EXPLICIT_MODULES := NO
endif
SWIFT_SOURCES := main.swift
CXX_SOURCES := impl.cpp
SWIFT_CXX_INTEROP := 1
SWIFTFLAGS_EXTRAS = -Xcc -I$(SRCDIR)
# The Embedded Swift link does not add the C++ standard library.
ifeq "$(SWIFT_EMBEDDED_MODE)" "1"
ifeq "$(OS)" "Darwin"
LD_EXTRAS := -lc++
else
LD_EXTRAS := -lstdc++
endif
endif
include Makefile.rules
