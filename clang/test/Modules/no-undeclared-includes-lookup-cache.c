// RUN: rm -rf %t
// RUN: split-file %s %t

//--- include/module.modulemap
module A [no_undeclared_includes] { textual header "A.h" }
module B { header "B.h" }
//--- include/A.h
// Even textual headers within module A now inherit [no_undeclared_includes] and
// thus do not have that include.
#if __has_include(<B.h>)
#endif
//--- include/B.h

//--- tu.c
#if !__has_include(<B.h>)
#error Main TU does have that include.
#endif

#include "A.h"

#if !__has_include(<B.h>)
#error Main TU still has that include.
// We hit the above because the unsuccessful __has_include check in A.h taints
// lookup cache (HeaderSearch::LookupFileCache) of this CompilerInstance.
#endif

// RUN: %clang -I %t/include -fmodules -fimplicit-module-maps \
// RUN:   -fmodules-cache-path=%t/cache -fsyntax-only %t/tu.c
