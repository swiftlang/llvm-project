// This test verifies modules that are entirely comprised from stable directory inputs are captured in
// dependency information when paths are prefix mapped.
 
// REQUIRES: shell,ondisk_cas

// RUN: rm -rf %t
// RUN: split-file %s %t
// RUN: sed -e "s|DIR|%/t|g" %t/overlay.json.template > %t/overlay.json
// RUN: sed -e "s|DIR|%/t|g" %t/compile.json.in > %t/compile.json

// RUN: clang-scan-deps -compilation-database %t/compile.json \
// RUN:   -prefix-map=%t/modules=/^modules -prefix-map=%t=/^src -prefix-map-sdk=/^sdk -prefix-map-toolchain=/^tc \
// RUN:   -cas-path %t/cas -module-files-dir %t/modules \
// RUN:   -j 1 -format experimental-full > %t/deps.db

// RUN: cat %t/deps.db | sed 's:\\\\\?:/:g' | FileCheck %s -DPREFIX=%/t --check-prefix DEP

// DEP: "is-in-stable-directories": true
// DEP: "name": "A"

// CHECK-NOT: "is-in-stable-directories": true
// CHECK-NOT: warning:
// CHECK-NOT: error:

//--- compile.json.in
[
{
    "directory": "DIR",
    "command": "clang -x c -c DIR/client.c -isysroot DIR/MacOSX.sdk -IDIR/BuildDir -ivfsoverlay DIR/overlay.json -IDIR/MacOSX.sdk/usr/include -fmodules -fmodules-cache-path=DIR/module-cache -fimplicit-module-maps -o DIR/client.c.o",
    "file": "DIR/client.c"
}
]

//--- overlay.json.template
{
  "version": 0,
  "case-sensitive": "false",
  "roots": [
    {
          "external-contents": "DIR/BuildDir/B_vfs.h",
          "name": "DIR/MacOSX.sdk/usr/include/B/B_vfs.h",
          "type": "file"
    }
  ]
}

//--- MacOSX.sdk/usr/include/A/module.modulemap
module A [system] {
  umbrella "."
}

//--- MacOSX.sdk/usr/include/A/A.h
typedef int A_type;

//--- MacOSX.sdk/usr/include/B/module.modulemap
module B [system] {
  umbrella "."
}

//--- MacOSX.sdk/usr/include/B/B.h
#include <B/B_vfs.h>

//--- BuildDir/B_vfs.h
typedef int local_t;

//--- MacOSX.sdk/usr/include/sys/sys.h
#include <A/A.h>
typedef int sys_t_m;

//--- MacOSX.sdk/usr/include/sys/module.modulemap
module sys [system] {
  umbrella "."
}

//--- MacOSX.sdk/usr/include/B_transitive/B.h
#include <B/B.h>

//--- MacOSX.sdk/usr/include/B_transitive/module.modulemap
module B_transitive [system] {
  umbrella "."
}

//--- MacOSX.sdk/usr/include/C/module.modulemap
module C [system] {
  umbrella "."
}

//--- MacOSX.sdk/usr/include/C/C.h
#include <B_transitive/B.h>


//--- client.c
#include <A/A.h>
#include <C/C.h> // This dependency transitively depends on a local header.
