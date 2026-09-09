SWIFT_SOURCES := main.swift

all: fakesdk a.out

include Makefile.rules

ifeq "$(OS)" "Windows_NT"
# A symbolic link (mklink /D) needs admin/Developer Mode, but a directory
# junction (/J) does not and is enough to point fakesdk at the real SDK.
fakesdk:
	cmd /c mklink /J "$(subst /,\,$@)" "$(subst /,\,$(SDKROOT))"

SWIFTFLAGS := $(subst $(SDKROOT),$(CURDIR)/fakesdk,$(SWIFTFLAGS))
else
fakesdk:
	$(call LN_SF,$(SDKROOT),$@)

SWIFTFLAGS := $(subst $(SDKROOT),$(shell pwd)/fakesdk,$(SWIFTFLAGS))
endif

clean::
	$(call RM_F,fakesdk a.out *.dSYM)
