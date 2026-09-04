# LLDB replays the compiler invocation recorded in the debug info. With explicit
# modules that names Lib.swiftmodule by path, so taking it away below would fail
# every import instead of only the types that come from Lib.
export SWIFT_ENABLE_EXPLICIT_MODULES := NO

SWIFT_SOURCES := main.swift
SWIFTFLAGS_EXTRAS := -I.
LD_EXTRAS = -L. -Xlinker -rpath -Xlinker $(BUILDDIR) -lLib

all: clean dylib a.out hide-module

include Makefile.rules

# DYLIB_HIDE_SWIFTMODULE together with no reflection metadata makes it
# impossible to lay out `self`.
.PHONY: dylib
dylib: Lib.swift \
       $(call tool_prereq,$(SWIFTC_PATH)) $(call tool_prereq,$(SWIFT_FRONTEND_PATH))
	"$(MAKE)" -f $(MAKEFILE_RULES) -C $(BUILDDIR) VPATH=$(SRCDIR) \
		CC=$(CC) SWIFTC=$(SWIFTC) ARCH=$(ARCH) DSYMUTIL=$(DSYMUTIL) \
		DYLIB_HIDE_SWIFTMODULE=YES \
		DYLIB_ONLY=YES \
		DYLIB_NAME=Lib \
		DYLIB_SWIFT_SOURCES=Lib.swift \
		SWIFTFLAGS_EXTRAS="-Xfrontend -disable-reflection-metadata -enable-library-evolution"

# Now that main.swift has been compiled against it, take away everything LLDB
# could reconstruct Lib.Root from.
.PHONY: hide-module
hide-module: a.out
	$(call RM_F,Lib.swiftmodule Lib.swiftinterface Lib.private.swiftinterface)
