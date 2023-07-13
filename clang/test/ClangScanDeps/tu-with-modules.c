// UNSUPPORTED: target=powerpc64-ibm-aix{{.*}}

// RUN: rm -rf %t
// RUN: split-file %s %t

//--- module.modulemap
module root { header "root.h" }
module direct { header "direct.h" }
module transitive { header "transitive.h" }
module addition { header "addition.h" }
//--- root.h
#include "direct.h"
#include "root/textual.h"
//--- direct.h
#include "transitive.h"
//--- transitive.h
// empty

//--- addition.h
// empty

//--- tu.c
#include "root.h"

//--- root/textual.h
// This is here to verify that the "root" directory doesn't clash with name of
// the "root" module.

//--- cdb.json.template
[{
  "file": "",
  "directory": "DIR",
  "command": "clang -fmodules -fmodules-cache-path=DIR/cache -I DIR -x c -c"
}]

// RUN: sed "s|DIR|%/t|g" %t/cdb.json.template > %t/cdb.json
// RUN: clang-scan-deps -compilation-database %t/cdb.json -format experimental-full -tu-path %t/tu.c > %t/result.json
// RUN: cat %t/result.json | sed 's:\\\\\?:/:g' | FileCheck -DPREFIX=%/t %s --check-prefix=CHECK
// RUN: clang-scan-deps -compilation-database %t/cdb.json -format experimental-full -tu-path %t/tu.c -import-module addition > %t/result.json
// RUN: cat %t/result.json | sed 's:\\\\\?:/:g' | FileCheck -DPREFIX=%/t %s --check-prefix=CHECK --check-prefix=ADDITION

// CHECK:      {
// CHECK-NEXT:   "modules": [
// ADDITION-NEXT:  {
// ADDITION-NEXT:    "clang-module-deps": [],
// ADDITION-NEXT:    "clang-modulemap-file": "[[PREFIX]]/module.modulemap",
// ADDITION-NEXT:    "command-line": [
// ADDITION:         ],
// ADDITION-NEXT:    "context-hash": "{{.*}}",
// ADDITION-NEXT:    "file-deps": [
// ADDITION-NEXT:      "[[PREFIX]]/./addition.h"
// ADDITION-NEXT:      "[[PREFIX]]/./module.modulemap"
// ADDITION-NEXT:    ],
// ADDITION-NEXT:    "name": "addition"
// ADDITION-NEXT:  },
// CHECK-NEXT:     {
// CHECK-NEXT:       "clang-module-deps": [
// CHECK-NEXT:         {
// CHECK-NEXT:           "context-hash": "{{.*}}",
// CHECK-NEXT:           "module-name": "transitive"
// CHECK-NEXT:         }
// CHECK-NEXT:       ],
// CHECK-NEXT:       "clang-modulemap-file": "[[PREFIX]]/module.modulemap",
// CHECK-NEXT:       "command-line": [
// CHECK:            ],
// CHECK-NEXT:       "context-hash": "{{.*}}",
// CHECK-NEXT:       "file-deps": [
// CHECK-NEXT:         "[[PREFIX]]/./direct.h"
// CHECK-NEXT:         "[[PREFIX]]/./module.modulemap"
// CHECK-NEXT:       ],
// CHECK-NEXT:       "name": "direct"
// CHECK-NEXT:     },
// CHECK-NEXT:     {
// CHECK-NEXT:       "clang-module-deps": [
// CHECK-NEXT:         {
// CHECK-NEXT:           "context-hash": "{{.*}}",
// CHECK-NEXT:           "module-name": "direct"
// CHECK-NEXT:         }
// CHECK-NEXT:       ],
// CHECK-NEXT:       "clang-modulemap-file": "[[PREFIX]]/module.modulemap",
// CHECK-NEXT:       "command-line": [
// CHECK:            ],
// CHECK-NEXT:       "context-hash": "{{.*}}",
// CHECK-NEXT:       "file-deps": [
// CHECK-NEXT:         "[[PREFIX]]/./module.modulemap"
// CHECK-NEXT:         "[[PREFIX]]/./root.h"
// CHECK-NEXT:         "[[PREFIX]]/./root/textual.h"
// CHECK-NEXT:       ],
// CHECK-NEXT:       "name": "root"
// CHECK-NEXT:     },
// CHECK-NEXT:     {
// CHECK-NEXT:       "clang-module-deps": [],
// CHECK-NEXT:       "clang-modulemap-file": "[[PREFIX]]/module.modulemap",
// CHECK-NEXT:       "command-line": [
// CHECK:            ],
// CHECK-NEXT:       "context-hash": "{{.*}}",
// CHECK-NEXT:       "file-deps": [
// CHECK-NEXT:         "[[PREFIX]]/./module.modulemap"
// CHECK-NEXT:         "[[PREFIX]]/./transitive.h"
// CHECK-NEXT:       ],
// CHECK-NEXT:       "name": "transitive"
// CHECK-NEXT:     }
// CHECK-NEXT:   ],
// CHECK-NEXT:   "translation-units": [
// CHECK-NEXT:     {
// CHECK-NEXT:       "commands": [
// CHECK-NEXT:         {
// CHECK-NEXT:           "clang-context-hash": "{{.*}}",
// CHECK-NEXT:           "clang-module-deps": [
// ADDITION-NEXT:          {
// ADDITION-NEXT:            "context-hash": "{{.*}}",
// ADDITION-NEXT:            "module-name": "addition"
// ADDITION-NEXT:          },
// CHECK-NEXT:             {
// CHECK-NEXT:               "context-hash": "{{.*}}",
// CHECK-NEXT:               "module-name": "root"
// CHECK-NEXT:             }
// CHECK-NEXT:           ],
// CHECK-NEXT:           "command-line": [
// CHECK:                ],
// CHECK-NEXT:           "executable": "clang",
// CHECK-NEXT:           "file-deps": [
// CHECK-NEXT:             "[[PREFIX]]/<clang-module-imports>"
// CHECK-NEXT:           ],
// CHECK-NEXT:           "input-file": ""
// CHECK-NEXT:         }
// CHECK-NEXT:       ]
// CHECK-NEXT:     }
// CHECK-NEXT:   ]
// CHECK-NEXT: }
