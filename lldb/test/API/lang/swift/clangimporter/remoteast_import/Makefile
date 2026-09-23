SRCDIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
SWIFT_SOURCES := main.swift
# Only compiles with -DSYNTAX_ERROR=1, which Library does not pass.
SWIFT_BRIDGING_HEADER := BridgingHeader.h
# This Makefile overwrites the default rules.
EXE := a.out

all: libLibrary a.out

include Makefile.rules

SWIFTFLAGS += -I. -Xcc -DSYNTAX_ERROR=1
LD_EXTRAS += -lLibrary -L.

.PHONY: libLibrary

libLibrary: \
            $(call tool_prereq,$(CC))
	"$(MAKE)" -f $(SRCDIR)/Library.mk VPATH=$(SRCDIR) -I $(SRCDIR)

clean::
	$(call RM_RF,*.swiftmodule *.swiftdoc *.dSYM *~ lib*.dylib a.out *.o)
