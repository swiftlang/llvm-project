SWIFT_SOURCES := main.swift

# -O is required: DW_OP_entry_value locations only exist in optimized code.
# The test asserts that the location really is an entry value, so it fails
# loudly rather than silently passing if the optimizer stops producing one.
SWIFTFLAGS_EXTRAS := -O

include Makefile.rules
