// This test checks that reading a non-affecting module map during PCM
// compilation doesn't cause the PCM to be recompiled when the file changes.

// RUN: rm -rf %t && mkdir %t
// RUN: split-file %s %t

// XFAIL: *
// rdar://101268970

//--- include/a/module.modulemap
module a {}
//--- include/a/a.h
// empty

//--- include/b/module.modulemap
module b { header "b.h" }
//--- include/b/b.h
#include "a/a.h"

//--- include/c/module.modulemap
module c { header "c.h" }
//--- include/c/c.h
#include "b/b.h"

//--- test-b.m
@import b; // expected-remark-re{{building module 'b' {{.*}}}} \
           // expected-remark{{finished building module 'b'}} \
           // expected-remark-re{{importing module 'b' from {{.*}}}}

//--- test-c.m
@import c; // expected-remark-re{{building module 'c' {{.*}}}} \
           // expected-remark{{finished building module 'c'}} \
           // expected-remark-re{{importing module 'c' from {{.*}}}} \
           // expected-remark-re{{importing module 'b' into 'c' from {{.*}}}}

//--- test-c-no-rebuild.m
@import c; // expected-remark-re{{importing module 'c' from {{.*}}}} \
           // expected-remark-re{{importing module 'b' into 'c' from {{.*}}}}

// Build module 'b' for the first time.
// RUN: %clang_cc1 -fmodules -fimplicit-module-maps -fmodules-cache-path=%t/cache -fdisable-module-hash -I %t/include %t/test-b.m -Rmodule-build -Rmodule-import -verify

// Verify that changing the mtime doesn't trigger re-build.
// RUN: touch %t/include/a/module.modulemap
// RUN: %clang_cc1 -fmodules -fimplicit-module-maps -fmodules-cache-path=%t/cache -fdisable-module-hash -I %t/include %t/test-c.m -Rmodule-build -Rmodule-import -verify

// Verify that modifying the contents doesn't trigger re-build.
// RUN: echo " " >> %t/include/a/module.modulemap
// RUN: %clang_cc1 -fmodules -fimplicit-module-maps -fmodules-cache-path=%t/cache -fdisable-module-hash -I %t/include %t/test-c-no-rebuild.m -Rmodule-build -Rmodule-import -verify

// Verify that removing the file doesn't trigger re-build.
// RUN: rm %t/include/a/module.modulemap
// RUN: %clang_cc1 -fmodules -fimplicit-module-maps -fmodules-cache-path=%t/cache -fdisable-module-hash -I %t/include %t/test-c-no-rebuild.m -Rmodule-build -Rmodule-import -verify

// Verify that existing signature still works after removing the 'a' module map.
// RUN: rm %t/cache/b.pcm
// RUN: %clang_cc1 -fmodules -fimplicit-module-maps -fmodules-cache-path=%t/cache -fdisable-module-hash -I %t/include %t/test-b.m -Rmodule-build -Rmodule-import -verify
// RUN: %clang_cc1 -fmodules -fimplicit-module-maps -fmodules-cache-path=%t/cache -fdisable-module-hash -I %t/include %t/test-c-no-rebuild.m -Rmodule-build -Rmodule-import -verify
//      ^^^ will first try to import 'c.pcm' with the stale signature and then rebuild it to reflect the new signature of 'b.pcm'

// RUN: false
