SWIFT_SOURCES := main.swift
SWIFT_OBJC_INTEROP := 1
SWIFTFLAGS_EXTRAS = -I$(SRCDIR)/Foo -L$(shell pwd) -lFoo -Xfrontend -enable-import-objc-forward-declarations
LD_FLAGS = some_nonesense

include Makefile.rules

a.out: libFoo.dylib

libFoo.dylib: $(SRCDIR)/Foo/foo.m
	$(CC) -framework Foundation -dynamiclib $(CFLAGS) -o $@ $<

clean::
	# rm -rf *.swiftmodule *.swiftdoc *.dSYM lib*.dylib a.out *.o
