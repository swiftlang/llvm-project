C_SOURCES := main.c
CFLAGS_EXTRAS = -g -I$(SRCDIR)
LD_EXTRAS = $(BUILDDIR)/cached/libcached.dylib $(BUILDDIR)/uncached/libuncached.dylib

all: a.out

include Makefile.rules

# Two dylibs built from the same headers. The cached one is compiled through
# the explicit-module driver with a CAS, so its DWARF refers to the .pcm by
# CASID; the uncached one is built the ordinary way, so its DWARF refers to the
# .pcm by path. Both get a .cas-config next to them pointing at a CAS of their
# own, so both resolve to a CAS -- but only the cached one loads a module out of
# one.
#
# Built with explicit rules rather than DYLIB_C_SOURCES because the cached
# dylib needs a bespoke compile step and neither may be run through dsymutil:
# that would resolve the -gmodules references and there would be no module
# left to load out of the CAS.

CAS_DIR = $(BUILDDIR)/cached
NOCAS_DIR = $(BUILDDIR)/uncached
MODULE_FLAGS = -fmodules -gmodules -I$(SRCDIR)
LINK_FLAGS = $(SYSROOT_FLAGS) $(ARCHFLAG) $(ARCH)
EXPLICIT_MODULE_BUILD = $(LLDB_BASE_DIR)/scripts/clang-explicit-module-build.py

a.out: $(CAS_DIR)/libcached.dylib $(NOCAS_DIR)/libuncached.dylib

$(CAS_DIR)/libcached.dylib: $(SRCDIR)/cached.c
	mkdir -p $(CAS_DIR)
	echo '{"CASPath": "$(CAS_DIR)/cas"}' > $(CAS_DIR)/.cas-config
	python3 $(EXPLICIT_MODULE_BUILD) -cas-path $(CAS_DIR)/cas -- \
	  $(CC) -c -g $(MODULE_FLAGS) -fmodules-cache-path=$(CAS_DIR)/module-cache \
	  -o $(CAS_DIR)/cached.o $(SRCDIR)/cached.c
	$(CC) -dynamiclib -g $(LINK_FLAGS) $(CAS_DIR)/cached.o -o $@

$(NOCAS_DIR)/libuncached.dylib: $(SRCDIR)/uncached.c
	mkdir -p $(NOCAS_DIR)
	echo '{"CASPath": "$(NOCAS_DIR)/cas"}' > $(NOCAS_DIR)/.cas-config
	$(CC) -c -g $(MODULE_FLAGS) -fmodules-cache-path=$(NOCAS_DIR)/module-cache \
	  -o $(NOCAS_DIR)/uncached.o $(SRCDIR)/uncached.c
	$(CC) -dynamiclib -g $(LINK_FLAGS) $(NOCAS_DIR)/uncached.o -o $@
