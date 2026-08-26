C_SOURCES := main.c

swift-async-decoy.o: swift-async-decoy.s
	$(CPP) -o swift-async-decoy.s $(SRCDIR)/swift-async-decoy.s
	$(CC) $(CFLAGS) -c -o swift-async-decoy.o swift-async-decoy.s

include Makefile.rules

a.out: swift-async-decoy.o

# Needs to come after include
OBJECTS += swift-async-decoy.o
