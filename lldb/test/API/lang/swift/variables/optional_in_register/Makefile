SWIFT_SOURCES := main.swift

# -O is required: the parameters only end up in registers (a DW_OP_regN
# location rather than a memory one) in optimized code. The test asserts that
# the location really is a register, so it fails loudly rather than silently
# passing if that stops being true.
SWIFTFLAGS_EXTRAS := -O

include Makefile.rules
