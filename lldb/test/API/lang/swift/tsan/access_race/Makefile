
SWIFT_SOURCES := main.swift

# Modules built from .swiftinterface under -sanitize=thread contain TSan
# instrumentation but share a cache key with uninstrumented builds.
USE_PRIVATE_MODULE_CACHE := YES

include Makefile.rules

SWIFTFLAGS += -sanitize=thread
