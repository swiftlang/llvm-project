
SWIFT_SOURCES := main.swift

# Modules built from .swiftinterface under -sanitize=thread contain TSan
# instrumentation but share a cache key with uninstrumented builds.
USE_PRIVATE_MODULE_CACHE := YES

# Note the lazy variable setting - needs to use CLANG_RT_DIR, defined in Makefile.rules
LD_EXTRAS = -lclang_rt.tsan_osx_dynamic -L$(CLANG_RT_DIR)

include Makefile.rules

SWIFTFLAGS += -sanitize=thread
