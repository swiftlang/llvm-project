// REQUIRES: ondisk_cas

// Include-tree scanning of a header reached through -ivfsoverlay with
// "use-external-names": false.
//
// This is the configuration where the name the compiler sees is a *virtual*
// path that exists in no filesystem below the overlay: the RedirectingFileSystem
// reports "vdir/virtual_header.h" while the bytes come from "real/actual_header.h".
// A CAS reference for such a file therefore cannot be recovered by re-resolving
// its reported path against a lower filesystem -- that path is unknown there,
// and in an overlay that remapped onto a different file it would name the wrong
// contents. The reference is instead derived from the contents the compilation
// actually read.
//
// Check that the scan succeeds, that the header is recorded under its virtual
// name, and that the resulting include tree replays and compiles -- which is
// what proves the recorded contents are the right bytes.

// RUN: rm -rf %t
// RUN: split-file %s %t
// RUN: sed -e "s|DIR|%/t|g" %t/vfs-overlay.yaml.template > %t/vfs-overlay.yaml
// RUN: sed -e "s|DIR|%/t|g" %t/cdb.json.template > %t/cdb.json

// RUN: clang-scan-deps -compilation-database %t/cdb.json -cas-path %t/cas \
// RUN:   -format experimental-include-tree \
// RUN:   > %t/result.txt
// RUN: cat %t/result.txt | sed -e "s|%/t|DIR|g" \
// RUN:   | FileCheck %s --implicit-check-not=actual_header.h

// The header appears in the include tree, and in the file list, under the
// virtual name. The external name never appears (see --implicit-check-not).
// CHECK: DIR{{/|\\}}t.c llvmcas://
// CHECK: DIR{{/|\\}}vdir{{/|\\}}virtual_header.h llvmcas://
// CHECK: Files:
// CHECK-DAG: DIR{{/|\\}}t.c llvmcas://
// CHECK-DAG: DIR{{/|\\}}vdir{{/|\\}}virtual_header.h llvmcas://

// The include tree replays. This only compiles if the contents recorded for the
// virtual path are the bytes of real/actual_header.h, i.e. if the CAS reference
// was taken from the contents rather than re-derived from the path.
// RUN: %clang -fdepscan=inline -Xclang -fcas-path -Xclang %t/cas \
// RUN:   -Xclang -ivfsoverlay -Xclang %t/vfs-overlay.yaml -I%t/vdir \
// RUN:   -c %t/t.c -o %t/t.o

//--- t.c
#include "virtual_header.h"
int test(struct S *s) { return s->x; }

//--- real/actual_header.h
struct S { int x; };

//--- vfs-overlay.yaml.template
version: 0
use-external-names: false
roots:
  - name: "DIR/vdir"
    type: directory
    contents:
      - name: virtual_header.h
        type: file
        external-contents: "DIR/real/actual_header.h"

//--- cdb.json.template
[{
  "directory": "DIR",
  "command": "clang -fsyntax-only DIR/t.c -ivfsoverlay DIR/vfs-overlay.yaml -I DIR/vdir",
  "file": "DIR/t.c"
}]
