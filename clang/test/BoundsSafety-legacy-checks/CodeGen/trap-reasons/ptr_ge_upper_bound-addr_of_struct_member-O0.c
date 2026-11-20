// RUN: echo "; __SEPERATOR__" > %t.sep

// RUN: %clang_cc1 -DTYPE=__bidi_indexable -O0 -debug-info-kind=standalone \
// RUN:   -dwarf-version=5 -fbounds-safety \
// RUN:   -fbounds-safety-debug-trap-reasons=basic -emit-llvm %s -o %t_bidi_basic.ll
// RUN: cat %t_bidi_basic.ll %t.sep %t_bidi_basic.ll > %t_bidi_basic.repeated.ll
// RUN: FileCheck %s --check-prefixes=CHECK,BASIC \
// RUN:   --input-file=%t_bidi_basic.repeated.ll

// RUN: %clang_cc1 -DTYPE=__bidi_indexable -O0 -debug-info-kind=standalone \
// RUN:   -dwarf-version=5 -fbounds-safety \
// RUN:   -fbounds-safety-debug-trap-reasons=detailed -emit-llvm %s -o %t_bidi_detailed.ll
// RUN: cat %t_bidi_detailed.ll %t.sep %t_bidi_detailed.ll > %t_bidi_detailed.repeated.ll
// RUN: FileCheck %s --check-prefixes=CHECK,DETAILED \
// RUN:   --input-file=%t_bidi_detailed.repeated.ll

// RUN: %clang_cc1 -DTYPE=__indexable -O0 -debug-info-kind=standalone \
// RUN:   -dwarf-version=5 -fbounds-safety \
// RUN:   -fbounds-safety-debug-trap-reasons=basic -emit-llvm %s -o %t_idx_basic.ll
// RUN: cat %t_idx_basic.ll %t.sep %t_idx_basic.ll > %t_idx_basic.repeated.ll
// RUN: FileCheck %s --check-prefixes=CHECK,BASIC \
// RUN:   --input-file=%t_idx_basic.repeated.ll

// RUN: %clang_cc1 -DTYPE=__indexable -O0 -debug-info-kind=standalone \
// RUN:   -dwarf-version=5 -fbounds-safety \
// RUN:   -fbounds-safety-debug-trap-reasons=detailed -emit-llvm %s -o %t_idx_detailed.ll
// RUN: cat %t_idx_detailed.ll %t.sep %t_idx_detailed.ll > %t_idx_detailed.repeated.ll
// RUN: FileCheck %s --check-prefixes=CHECK,DETAILED \
// RUN:   --input-file=%t_idx_detailed.repeated.ll

#include <ptrcheck.h>

#ifndef TYPE
#error TYPE must be defined
#endif

typedef struct Data {
    int a;
    int b;
} Data_t;

int* TYPE operation(Data_t* TYPE d) {
    return &d->a;
}

// In first copy of the file
// CHECK-DAG: [[OPT_REMARK:![0-9]+]] = !{!"bounds-safety-check-ptr-lt-upper-bound"}
// BASIC-DAG: [[TRAP_SCOPE:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$Pointer to struct above bounds while taking address of struct member", scope: [[FILE_SCOPE:![0-9]+]], file: [[FILE_SCOPE:![0-9]+]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// DETAILED-DAG: [[TRAP_SCOPE:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$taking address of struct member above upper bound in '&d->a'", scope: [[FILE_SCOPE:![0-9]+]], file: [[FILE_SCOPE:![0-9]+]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// CHECK-DAG: [[TRAP_LOC:![0-9]+]] = !DILocation(line: 0, scope: [[TRAP_SCOPE]], inlinedAt: [[SRC_LOC:![0-9]+]])
// CHECK-DAG: [[SRC_LOC]] = !DILocation(line: 43, column: 12, scope: {{![0-9]+}})
// CHECK-LABEL: ; __SEPERATOR__

// In second copy of the file
// CHECK-LABEL: @operation
// Note: We use the `OPT_REMARK` to make sure we match against the correct branch.
// Unfortunately `SRC_LOC` is not enough because it isn't unique.
// CHECK: br i1 %{{.+}}, label %{{.+}}, label %[[TRAP_LABEL:[a-z0-9]+]], !dbg [[SRC_LOC]], !prof ![[PROFILE_METADATA:[0-9]+]], !annotation [[OPT_REMARK]]
// CHECK: [[TRAP_LABEL]]:
// CHECK-NEXT: call void @llvm.ubsantrap(i8 25) {{.*}}!dbg [[TRAP_LOC]]
