// REQUIRES: ondisk_cas
// REQUIRES: x86-registered-target

// RUN: rm -rf %t
// RUN: split-file %s %t
// RUN: sed -e "s|DIR|%/t|g" -e "s|CLANG|%/clang|g" %t/cdb.json.template > %t/cdb.json
// RUN: clang-scan-deps -compilation-database %t/cdb.json \
// RUN:   -cas-path %t/cas -module-files-dir %t/outputs \
// RUN:   -format experimental-include-tree-full \
// RUN:   -mode preprocess-dependency-directives -optimize-args=none > %t/deps.json

// RUN: %deps-to-rsp %t/deps.json --module-name Base > %t/Base.rsp
// RUN: %deps-to-rsp %t/deps.json --module-name Left > %t/Left.rsp
// RUN: %deps-to-rsp %t/deps.json --module-name Right > %t/Right.rsp
// RUN: %deps-to-rsp %t/deps.json --tu-index 0 > %t/tu.rsp

// Build Base into CAS using the scanner-produced include-tree command.
// RUN: %clang @%t/Base.rsp 2>&1 | FileCheck %s

// Build two explicit modules from the filesystem. Both recover Base from the
// same CAS key, but expose it at a path relative to their own module directory.
// RUN: sed -E -e 's| "-fcas-include-tree" "[^"]+"||' \
// RUN:   -e 's| "-fcache-compile-job"||' \
// RUN:   -e 's|"-fmodule-file=Base=Base-[^"]*\.pcm"|"-fmodule-file=%/t/Left/Base.pcm"|' \
// RUN:   -e 's|"Base-[^"]*\.pcm"|"%/t/Left/Base.pcm"|g' \
// RUN:   -e 's|"-o" "[^"]*"|"-o" "%/t/Left.pcm"|' \
// RUN:   %t/Left.rsp > %t/Left.fs.rsp
// RUN: echo '"%/t/Left/module.modulemap"' >> %t/Left.fs.rsp
// RUN: sed -E -e 's| "-fcas-include-tree" "[^"]+"||' \
// RUN:   -e 's| "-fcache-compile-job"||' \
// RUN:   -e 's|"-fmodule-file=Base=Base-[^"]*\.pcm"|"-fmodule-file=%/t/Right/Base.pcm"|' \
// RUN:   -e 's|"Base-[^"]*\.pcm"|"%/t/Right/Base.pcm"|g' \
// RUN:   -e 's|"-o" "[^"]*"|"-o" "%/t/Right.pcm"|' \
// RUN:   %t/Right.rsp > %t/Right.fs.rsp
// RUN: echo '"%/t/Right/module.modulemap"' >> %t/Right.fs.rsp
// RUN: %clang @%t/Left.fs.rsp
// RUN: %clang @%t/Right.fs.rsp

// Load both explicit modules from the filesystem. Their shared dependency is
// available only through its direct CAS ID. It must be loaded as Base.pcm both
// times, rather than relative to the two importing module directories.
// RUN: sed -E -e 's| "-fcas-include-tree" "[^"]+"||' \
// RUN:   -e 's| "-fcache-compile-job"||' \
// RUN:   -e 's| "-fmodule-file-cache-key" "[^"]+" "[^"]+"||g' \
// RUN:   -e 's| "-fmodule-file=Left=[^"]+"||g' \
// RUN:   -e 's| "-fmodule-file=Right=[^"]+"||g' \
// RUN:   %t/tu.rsp > %t/tu.fs.rsp
// RUN: echo '"-fmodule-file=%/t/Left.pcm" "-fmodule-file=%/t/Right.pcm" "%/t/tu.c"' >> %t/tu.fs.rsp
// RUN: %clang @%t/tu.fs.rsp -verify

// CHECK: compile job cache miss

//--- cdb.json.template
+[{
  "file": "DIR/tu.c",
  "directory": "DIR",
  "command": "CLANG -fsyntax-only DIR/tu.c -I DIR/Base -I DIR/Left -I DIR/Right -fmodules -fimplicit-modules -fimplicit-module-maps -fmodules-cache-path=DIR/module-cache -Rcompile-job-cache"
}]

//--- Base/module.modulemap
module Base { header "Base.h" export * }

//--- Base/Base.h
void base(void);

//--- Left/module.modulemap
module Left { header "Left.h" export * }

//--- Left/Left.h
#pragma clang module import Base
void left(void);

//--- Right/module.modulemap
module Right { header "Right.h" export * }

//--- Right/Right.h
#pragma clang module import Base
void right(void);

//--- tu.c
// expected-no-diagnostics
#pragma clang module import Left
#pragma clang module import Right

void tu(void) {
  base();
  left();
  right();
}
