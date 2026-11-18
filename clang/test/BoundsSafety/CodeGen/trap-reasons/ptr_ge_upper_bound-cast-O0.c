// RUN: echo "; __SEPARATOR__" > %t.sep

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
// RUN: FileCheck %s --check-prefixes=CHECK,DETAILED,DETAILED-BIDI \
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
// RUN: FileCheck %s --check-prefixes=CHECK,DETAILED,DETAILED-IDX \
// RUN:   --input-file=%t_idx_detailed.repeated.ll

#include <ptrcheck.h>

#ifndef TYPE
#error TYPE must be defined
#endif

int* __single operation(int* TYPE i) {
    return i;
}
// In first copy of the file

// CHECK-DAG: [[OPT_REMARK:![0-9]+]] = !{!"bounds-safety-check-ptr-le-upper-bound"}
// BASIC-DAG: [[TRAP_SCOPE:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$Pointer above bounds while casting", scope: [[FILE_SCOPE:![0-9]+]], file: [[FILE_SCOPE:![0-9]+]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// DETAILED-BIDI-DAG: [[TRAP_SCOPE:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$converting 'int *__bidi_indexable' to 'int *__single' where pointer 'i' is above its upper bound", scope: [[FILE_SCOPE:![0-9]+]], file: [[FILE_SCOPE:![0-9]+]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// DETAILED-IDX-DAG: [[TRAP_SCOPE:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$converting 'int *__indexable' to 'int *__single' where pointer 'i' is above its upper bound", scope: [[FILE_SCOPE:![0-9]+]], file: [[FILE_SCOPE:![0-9]+]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// CHECK-DAG: [[TRAP_LOC:![0-9]+]] = !DILocation(line: 0, scope: [[TRAP_SCOPE]], inlinedAt: [[SRC_LOC:![0-9]+]])
// CHECK-DAG: [[SRC_LOC]] = !DILocation(line: 38, column: 12, scope: {{![0-9]+}})

// In the detailed mode the address space overflow gets its own trap reason.
// DETAILED-BIDI-DAG: [[TRAP_SCOPE_2:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$converting 'int *__bidi_indexable' to 'int *__single' where pointer 'i' overflows the address space", scope: [[FILE_SCOPE:![0-9]+]], file: [[FILE_SCOPE:![0-9]+]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// DETAILED-IDX-DAG: [[TRAP_SCOPE_2:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$converting 'int *__indexable' to 'int *__single' where pointer 'i' overflows the address space", scope: [[FILE_SCOPE:![0-9]+]], file: [[FILE_SCOPE:![0-9]+]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// DETAILED-DAG: [[TRAP_LOC_2:![0-9]+]] = !DILocation(line: 0, scope: [[TRAP_SCOPE_2]], inlinedAt: [[SRC_LOC]])
// CHECK-LABEL: ; __SEPARATOR__

// In second copy of the file

// CHECK-LABEL: @operation
// Note: We use the `OPT_REMARK` to make sure we match against the correct branch.
// FIXME: The overflow check should have its own opt-remark (rdar://150322607)
// CHECK: br i1 %{{.+}}, label %{{.+}}, label %[[TRAP_LABEL_0:[a-z0-9]+]], !dbg [[SRC_LOC]], !prof !{{[0-9]+}}, !annotation [[OPT_REMARK]]
// CHECK: [[TRAP_LABEL_0]]:
// CHECK-NEXT: call void @llvm.ubsantrap(i8 25) {{.*}}!dbg [[TRAP_LOC]]

// CHECK: br i1 %{{.+}}, label %{{.+}}, label %[[TRAP_LABEL_1:[a-z0-9]+]], !dbg [[SRC_LOC]], !prof !{{[0-9]+}}, !annotation [[OPT_REMARK]]
// CHECK: [[TRAP_LABEL_1]]:
// BASIC-NEXT: call void @llvm.ubsantrap(i8 25) {{.*}}!dbg [[TRAP_LOC]]
// DETAILED-NEXT: call void @llvm.ubsantrap(i8 25) {{.*}}!dbg [[TRAP_LOC_2]]
