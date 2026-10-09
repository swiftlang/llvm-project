SWIFT_SOURCES := main.swift
SWIFTFLAGS_EXTRAS := -I.
LD_EXTRAS = -L. -Xlinker -rpath -Xlinker $(BUILDDIR) -lLib

all: dylib a.out

include Makefile.rules

# Mimic a dyld shared cache image: drop the local symbols (-x) and the debug
# map (-S), and don't produce a dSYM.
.PHONY: dylib
dylib: Lib.swift \
       $(call tool_prereq,$(SWIFTC_PATH)) $(call tool_prereq,$(SWIFT_FRONTEND_PATH))
	"$(MAKE)" -f $(MAKEFILE_RULES) -C $(BUILDDIR) VPATH=$(SRCDIR) \
		CC=$(CC) SWIFTC=$(SWIFTC) ARCH=$(ARCH) DSYMUTIL=$(DSYMUTIL) \
		MAKE_DSYM=NO \
		DYLIB_ONLY=YES \
		DYLIB_NAME=Lib \
		DYLIB_SWIFT_SOURCES=Lib.swift \
		LD_EXTRAS="-Xlinker -x -Xlinker -S"
