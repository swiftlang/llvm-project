C_SOURCES := main.c
LD_EXTRAS := -L. -la -lb -lc -ld -le

a.out: liba libb libc libd libe

include Makefile.rules

liba:
	"$(MAKE)" -f $(MAKEFILE_RULES) \
		DYLIB_ONLY=YES DYLIB_C_SOURCES=lib.c DYLIB_NAME=a

libb:
	"$(MAKE)" -f $(MAKEFILE_RULES) \
		DYLIB_ONLY=YES DYLIB_C_SOURCES=lib.c DYLIB_NAME=b

libc:
	"$(MAKE)" -f $(MAKEFILE_RULES) \
		DYLIB_ONLY=YES DYLIB_C_SOURCES=lib.c DYLIB_NAME=c

libd:
	"$(MAKE)" -f $(MAKEFILE_RULES) \
		DYLIB_ONLY=YES DYLIB_C_SOURCES=lib.c DYLIB_NAME=d

libe:
	"$(MAKE)" -f $(MAKEFILE_RULES) \
		DYLIB_ONLY=YES DYLIB_C_SOURCES=lib.c DYLIB_NAME=e