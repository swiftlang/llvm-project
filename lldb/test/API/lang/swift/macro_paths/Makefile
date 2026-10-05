export SWIFT_ENABLE_EXPLICIT_MODULES := NO
SWIFT_SOURCES := main.swift
MAIN_IMPORT_FLAGS ?= -DIMPORT_MACRO
MAIN_PLUGIN_FLAGS ?= $(PLUGIN_FLAGS)
PLUGIN_IMPL ?= libMacroImpl.dylib
BUILD_PREFIX ?= /__lldb_macro_build__
SWIFTFLAGS_EXTRAS = -I$(BUILDDIR) $(MAIN_IMPORT_FLAGS) $(MAIN_PLUGIN_FLAGS) $(MAIN_DEBUG_FLAGS) $(PREFIX_FLAGS)

all: $(EXE)

include Makefile.rules

PREFIX_FLAGS = -Xfrontend -serialize-debugging-options \
               -Xfrontend -prefix-serialized-debugging-options \
               -debug-prefix-map $(BUILDDIR)=$(BUILD_PREFIX) \
               -debug-prefix-map $(PLUGIN_SERVER)=/__lldb_macro_toolchain__/swift-plugin-server
HOST_FLAGS = -I$(SWIFT_HOSTDIR) -L$(SWIFT_HOSTDIR) \
             -Xlinker -rpath -Xlinker $(SWIFT_HOSTDIR) \
             -target $(shell uname -m)-apple-macos13.0 \
             -sdk $(shell xcrun --sdk macosx --show-sdk-path)

$(SWIFT_OBJECTS): | Macro.swiftmodule

Macro.swiftmodule: Macro.swift $(PLUGIN_IMPL)
	$(SWIFTC) -emit-module -module-name Macro -o $@ $(SWIFT_MODULE_CACHE_FLAGS) \
		-sdk "$(SWIFTSDKROOT)" $(TARGET_SWIFTFLAGS) $(PLUGIN_FLAGS) $(PREFIX_FLAGS) $<

libMacroImpl.dylib: MacroImpl.swift
	$(SWIFTC) -emit-library -module-name MacroImpl -o $@ $(HOST_FLAGS) $<

MacroPlugin: MacroImpl.swift Plugin.swift
	$(SWIFTC) -parse-as-library -module-name MacroImpl -o $@ $(HOST_FLAGS) $^
