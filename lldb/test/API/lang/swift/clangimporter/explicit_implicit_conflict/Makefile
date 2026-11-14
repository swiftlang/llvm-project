SWIFT_SOURCES := main.swift
SWIFTFLAGS_EXTRAS = -I$(SRCDIR)/clang -I.
LD_EXTRAS = Lib.swift.o -Xlinker -add_ast_path -Xlinker $(BUILDDIR)/Lib.swiftmodule \
	-L$(BUILDDIR) -lImplicit

all: Lib.swift.o libImplicit $(EXE)

include Makefile.rules

# Built with explicit modules and only depends on Avail.
Lib.swift.o: Lib.swift
	"$(MAKE)" MAKE_DSYM=$(MAKE_DSYM) CC=$(CC) SWIFTC=$(SWIFTC) \
		ARCH=$(ARCH) DSYMUTIL=$(DSYMUTIL) \
		SWIFT_SOURCES=Lib.swift MODULENAME=Lib \
		SWIFTFLAGS_EXTRAS="-I$(SRCDIR)/clang -parse-as-library" \
		VPATH=$(SRCDIR) -I $(SRCDIR) -f $(MAKEFILE_RULES) Lib.swift.o

# Built with implicit modules.
libImplicit: Implicit.swift
	"$(MAKE)" MAKE_DSYM=YES CC=$(CC) SWIFTC=$(SWIFTC) \
		ARCH=$(ARCH) DSYMUTIL=$(DSYMUTIL) \
		SWIFT_ENABLE_EXPLICIT_MODULES=NO \
		DYLIB_NAME=Implicit DYLIB_SWIFT_SOURCES=Implicit.swift \
		VPATH=$(SRCDIR) SWIFTFLAGS_EXTRAS=-I$(SRCDIR)/clang \
		-f $(MAKEFILE_RULES) all
