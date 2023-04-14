// RUN: rm -rf %t
// RUN: split-file %s %t

// RUN: sed "s|DIR|%/t|g" %t/cdb.json.debug.template > %t/cdb.debug.json
// RUN: sed "s|DIR|%/t|g" %t/cdb.json.release.template > %t/cdb.release.json

// RUN: clang-scan-deps -compilation-database %t/cdb.debug.json -format=experimental-full -brief | FileCheck %s
// RUN: clang-scan-deps -compilation-database %t/cdb.release.json -format=experimental-full -brief | FileCheck %s
// CHECK: num modules: 1

//--- cdb.json.debug.template
[{
  "directory": "DIR",
  "file": "DIR/tu.c",
  "command": "clang -target x86_64-apple-macosx12 -x c -fmodules -gmodules -fmodules-cache-path=DIR/cache -I DIR/include -c DIR/tu.c -o DIR/tu.o -O0 -g"
},
{
  "directory": "DIR",
  "file": "DIR/tu.prefix.h",
  "command": "clang -target x86_64-apple-macosx12 -x c-header -fmodules -gmodules -fmodules-cache-path=DIR/cache -I DIR/include -c DIR/tu.prefix.h -o DIR/tu.pch -O0 -g"
}]
//--- cdb.json.release.template
[{
  "directory": "DIR",
  "file": "DIR/tu.c",
  "command": "clang -target x86_64-apple-macosx12 -x c -fmodules -gmodules -fmodules-cache-path=DIR/cache -I DIR/include -c DIR/tu.c -o DIR/tu.o -Os -g"
},
{
  "directory": "DIR",
  "file": "DIR/tu.prefix.h",
  "command": "clang -target x86_64-apple-macosx12 -x c-header -fmodules -gmodules -fmodules-cache-path=DIR/cache -I DIR/include -c DIR/tu.prefix.h -o DIR/tu.pch -Os -g"
}]

//--- include/module.modulemap
module Top { header "top.h" }
//--- include/top.h
#define TOP int
//--- tu.c
#include "top.h"
TOP fn(void);
//--- tu.prefix.h
#include "top.h"
