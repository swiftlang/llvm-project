SWIFT_SOURCES := main.swift

SWIFTFLAGS_EXTRAS := -I.

# Parameter packs in generic types require a newer runtime.
SWIFT_DEPLOYMENT_TARGET := 26

LD_EXTRAS = -L. -Xlinker -rpath -Xlinker $(BUILDDIR) -lOpaqueLib

all: dylib $(EXE)
.PHONY: dylib

include Makefile.rules

# MAKE_DSYM=NO and stripping local symbols together take the hidden opaque
# type descriptor of the library's internal declaration out of reach of the
# debugger. Stripping invalidates the code signature.
dylib: lib.swift
	"$(MAKE)" -f $(MAKEFILE_RULES) \
		MAKE_DSYM=NO DYLIB_ONLY=YES DYLIB_NAME=OpaqueLib \
		DYLIB_SWIFT_SOURCES="lib.swift"
	$(STRIP) -x "$(BUILDDIR)/libOpaqueLib.dylib"
ifneq "$(CODESIGN)" ""
	$(CODESIGN) -fs - "$(BUILDDIR)/libOpaqueLib.dylib"
endif
