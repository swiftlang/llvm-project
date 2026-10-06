SWIFT_SOURCES := main.swift
EXE := good.out

all: $(EXE) mismatch.out

include Makefile.rules

# A Swift version the compiler embedded in LLDB can never match.
FUTURE_SWIFT := Future Swift (swiftlang-9999.8.7.6)

# No compiler flag can claim a binary was built by a different compiler, so
# reach into the IR and rewrite the producer on the way to the object file.
mismatch.ll: $(SRCDIR)/main.swift \
             $(call tool_prereq,$(SWIFTC_PATH)) \
             $(call tool_prereq,$(SWIFT_FRONTEND_PATH))
	$(SWIFTC) $(SWIFTFLAGS) -emit-ir -module-name main $< -o $@.orig
	$(PYTHON) $(SRCDIR)/rewrite-producer.py $@.orig $@ "$(FUTURE_SWIFT)"

mismatch.o: mismatch.ll $(call tool_prereq,$(CC))
	$(CC) $(CFLAGS_NO_DEBUG) -Wno-override-module -c $< -o $@

mismatch.out: mismatch.o $(call tool_prereq,$(SWIFTC_PATH))
	$(SWIFTC) $(SWIFT_LINK_FLAGS) $< -o $@
ifneq "$(CODESIGN)" ""
	$(CODESIGN) -s - "$@"
endif

clean::
	$(call RM_F,mismatch.ll mismatch.ll.orig mismatch.o mismatch.out)
