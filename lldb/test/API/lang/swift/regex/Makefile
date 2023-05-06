SWIFT_SOURCES := main.swift

COMMON_SWIFTFLAGS := -enable-bare-slash-regex -Xfrontend -disable-availability-checking
ifeq "$(OS)" "Darwin"
	SWIFTFLAGS_EXTRAS := -target $(ARCH)-apple-macosx13.0 $(COMMON_SWIFTFLAGS)
else
	SWIFTFLAGS_EXTRAS := $(COMMON_SWIFTFLAGS)
endif

include Makefile.rules
