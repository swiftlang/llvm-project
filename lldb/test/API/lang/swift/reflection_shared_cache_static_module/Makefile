SWIFT_SOURCES := main.swift
SWIFTFLAGS_EXTRAS = -I$(BUILDDIR)
LD_EXTRAS = -L$(BUILDDIR) -lLib

all: libLib.a $(EXE)

include Makefile.rules

libLib.a: Lib.swift \
          $(call tool_prereq,$(SWIFTC_PATH)) $(call tool_prereq,$(SWIFT_FRONTEND_PATH))
	"$(MAKE)" -f $(MAKEFILE_RULES) \
		DYLIB_NAME=static \
		SWIFT_SOURCES=Lib.swift \
		MODULENAME=Lib \
		Lib.swift.o Lib.swiftmodule
	ar -r $@ $(BUILDDIR)/Lib.swift.o
