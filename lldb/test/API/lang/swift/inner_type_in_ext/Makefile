SWIFT_SOURCES := main.swift

all: libModBase.dylib a.out

include Makefile.rules
LD_EXTRAS = -lModBase -L$(BUILDDIR)
SWIFTFLAGS_EXTRAS = -I$(BUILDDIR)

libModBase.dylib: ModBase.swift
	"$(MAKE)" MAKE_DSYM=YES CC=$(CC) SWIFTC=$(SWIFTC) \
		ARCH=$(ARCH) DSYMUTIL=$(DSYMUTIL) \
		BASENAME=ModBase \
		SWIFTFLAGS_EXTRAS="-I$(BUILDDIR)" \
		VPATH=$(SRCDIR) -I $(SRCDIR) \
		DYLIB_ONLY:=YES DYLIB_NAME=ModBase \
		DYLIB_SWIFT_SOURCES:=ModBase.swift \
		-f $(MAKEFILE_RULES)
