EXE := main
SWIFT_SOURCES := main.swift

ifeq "$(OS)" "Windows_NT"
LD_EXTRAS = -Xlinker /LIBPATH:$(BUILDDIR) AA.lib BB.lib CC.lib
BB_SWIFTFLAGS_EXTRAS = -I$(BUILDDIR) -enable-library-evolution -static
BB_LD_EXTRAS = -Xlinker /LIBPATH:$(BUILDDIR)
else
LD_EXTRAS = -L$(BUILDDIR) -lAA -lBB -lCC
BB_SWIFTFLAGS_EXTRAS = -I$(BUILDDIR)
BB_LD_EXTRAS = -L$(BUILDDIR)
endif

SWIFTFLAGS_EXTRAS = -I$(BUILDDIR)
SWIFT_MODULE_CACHE_FLAGS = -module-cache-path MCP

# setup `all` to run each of the below before building the main executable
all: setup staticA staticB staticC clear_modules

include Makefile.rules

# Copy the source files into the build directory because we delete them, to be
# really sure we only have/use the .dylibs and .swiftinterfaces.
setup:
	$(call MKDIR_P,libs)
	$(call CP,$(SRCDIR)/libs/A.swift,libs/AA.swift)
	$(call CP,$(SRCDIR)/libs/B.swift,libs/BB.swift)
	$(call CP,$(SRCDIR)/libs/C.swift,libs/CC.swift)

# Because we override VPATH to point to the build directory, we need to pass
# down SWIFTC too (its default value is relative to VPATH).
staticA:
	"$(MAKE)" MAKE_DSYM=$(MAKE_DSYM) CC=$(CC) SWIFTC=$(SWIFTC) \
		ARCH=$(ARCH) DSYMUTIL=$(DSYMUTIL) VPATH=$(BUILDDIR) \
		SWIFTSDKROOT="$(SWIFTSDKROOT)" \
		SWIFT_MODULE_CACHE_FLAGS="$(SWIFT_MODULE_CACHE_FLAGS)" \
		BASENAME=AA -f $(SRCDIR)/libs/Makefile static_only

# BB imports AA, so it needs the build dir on its search path. A command-line
# assignment overrides the sub-Makefile's own SWIFTFLAGS_EXTRAS outright, so
# pass its flags through here too rather than losing them.
staticB:
	"$(MAKE)" MAKE_DSYM=$(MAKE_DSYM) CC=$(CC) SWIFTC=$(SWIFTC) \
		ARCH=$(ARCH) DSYMUTIL=$(DSYMUTIL) VPATH=$(BUILDDIR) \
		SWIFTSDKROOT="$(SWIFTSDKROOT)" \
		SWIFT_MODULE_CACHE_FLAGS="$(SWIFT_MODULE_CACHE_FLAGS)" \
		BASENAME=BB SWIFTFLAGS_EXTRAS="$(BB_SWIFTFLAGS_EXTRAS)" \
		LD_EXTRAS="$(BB_LD_EXTRAS)" -f $(SRCDIR)/libs/Makefile static_only

staticC:
	"$(MAKE)" MAKE_DSYM=$(MAKE_DSYM) CC=$(CC) SWIFTC=$(SWIFTC) \
		ARCH=$(ARCH) DSYMUTIL=$(DSYMUTIL) VPATH=$(BUILDDIR) \
		SWIFTSDKROOT="$(SWIFTSDKROOT)" \
		SWIFT_MODULE_CACHE_FLAGS="$(SWIFT_MODULE_CACHE_FLAGS)" \
		BASENAME=CC -f $(SRCDIR)/libs/Makefile static_only

# Make sure we only have .swiftinterface files for the generated modules:
# remove the copied sources and the swiftmodules from the build directory.
clear_modules:
	$(call RM_F,*.swiftmodule)
	$(call RM_RF,libs)
	$(call RM_RF,MCP)

clean::
	"$(MAKE)" VPATH=$(BUILDDIR) SWIFTC=$(SWIFTC) BASENAME=AA -f $(SRCDIR)/libs/Makefile clean
	"$(MAKE)" VPATH=$(BUILDDIR) SWIFTC=$(SWIFTC) BASENAME=BB -f $(SRCDIR)/libs/Makefile clean
	"$(MAKE)" VPATH=$(BUILDDIR) SWIFTC=$(SWIFTC) BASENAME=CC -f $(SRCDIR)/libs/Makefile clean
	$(call RM_RF,libs)
	$(call RM_RF,MCP)
