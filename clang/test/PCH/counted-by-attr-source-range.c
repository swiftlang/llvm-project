// Check that the bounds attribute's source range survives serialization, so the
// "consider using '__sized_by'" note and its fix-it are still produced for a
// declaration that came from a PCH rather than from a textual include.

// RUN: rm -rf %t && mkdir -p %t

// Baseline: textual include.
// RUN: not %clang_cc1 -fsyntax-only -I%S/Inputs -fdiagnostics-parseable-fixits \
// RUN:   -DTEXTUAL %s 2>&1 | FileCheck --check-prefix=TEXTUAL %s

// Same declarations, reached through a PCH.
// RUN: %clang_cc1 -emit-pch -o %t/cb.pch -x c-header %S/Inputs/counted-by-incomplete-pointee.h
// RUN: not %clang_cc1 -fsyntax-only -include-pch %t/cb.pch \
// RUN:   -fdiagnostics-parseable-fixits %s 2>&1 | FileCheck --check-prefix=PCH %s

// And through a module, which is how annotated SDK headers are normally consumed.
// RUN: not %clang_cc1 -fsyntax-only -fmodules -fimplicit-module-maps \
// RUN:   -fmodules-cache-path=%t/mcp -I%S/Inputs -DMODULE \
// RUN:   -fdiagnostics-parseable-fixits %s 2>&1 | FileCheck --check-prefix=PCH %s

#if defined(TEXTUAL) || defined(MODULE)
#include "counted-by-incomplete-pointee.h"
#endif

void use(struct CBBuf *p) {
  struct IncompleteTy *local = p->buf;
  (void)local;
}

// TEXTUAL: counted-by-attr-source-range.c:26:32: error: cannot use 'p->buf'
// TEXTUAL: counted-by-incomplete-pointee.h:3:1: note: consider providing a complete definition
// TEXTUAL: counted-by-incomplete-pointee.h:7:24: note: consider using '__sized_by' instead of '__counted_by'
// TEXTUAL: fix-it:{{.*}}counted-by-incomplete-pointee.h{{.*}}:{7:24-7:36}:"__sized_by"

// Reached through a PCH the diagnostics are identical, including the fix-it.
// PCH: counted-by-attr-source-range.c:26:32: error: cannot use 'p->buf'
// PCH: counted-by-incomplete-pointee.h:3:1: note: consider providing a complete definition
// PCH: counted-by-incomplete-pointee.h:7:24: note: consider using '__sized_by' instead of '__counted_by'
// PCH: fix-it:{{.*}}counted-by-incomplete-pointee.h{{.*}}:{7:24-7:36}:"__sized_by"
