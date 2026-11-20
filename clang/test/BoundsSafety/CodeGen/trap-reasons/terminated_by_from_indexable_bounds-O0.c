// RUN: echo "; __SEPERATOR__" > %t.sep

// =============================================================================
// Null terminator
// =============================================================================

// RUN: %clang_cc1 -O0 -debug-info-kind=standalone -dwarf-version=5 \
// RUN:   -fbounds-safety -fbounds-safety-debug-trap-reasons=basic \
// RUN:   -emit-llvm %s -o %t.ll
// RUN: cat %t.ll %t.sep %t.ll > %t.repeated.ll
// RUN: FileCheck %s --check-prefixes=CHECK,BASIC --input-file=%t.repeated.ll

// RUN: %clang_cc1 -O0 -debug-info-kind=standalone -dwarf-version=5 \
// RUN:   -fbounds-safety -fbounds-safety-debug-trap-reasons=detailed \
// RUN:   -emit-llvm %s -o %t2.ll
// RUN: cat %t2.ll %t.sep %t2.ll > %t2.repeated.ll
// RUN: FileCheck %s --check-prefixes=CHECK,DETAILED-NULL \
// RUN:   --input-file=%t2.repeated.ll

// =============================================================================
// Non-null terminator
// =============================================================================

// RUN: %clang_cc1 -O0 -debug-info-kind=standalone -dwarf-version=5 \
// RUN:   -fbounds-safety -fbounds-safety-debug-trap-reasons=basic \
// RUN:   -emit-llvm -DNON_NULL_TERM %s -o %t3.ll
// RUN: cat %t3.ll %t.sep %t3.ll > %t3.repeated.ll
// RUN: FileCheck %s --check-prefixes=CHECK,BASIC --input-file=%t3.repeated.ll

// RUN: %clang_cc1 -O0 -debug-info-kind=standalone -dwarf-version=5 \
// RUN:   -fbounds-safety -fbounds-safety-debug-trap-reasons=detailed \
// RUN:   -emit-llvm -DNON_NULL_TERM %s -o %t4.ll
// RUN: cat %t4.ll %t.sep %t4.ll > %t4.repeated.ll
// RUN: FileCheck %s --check-prefixes=CHECK,DETAILED-NON-NULL \
// RUN:   --input-file=%t4.repeated.ll

#include <ptrcheck.h>

#ifndef NON_NULL_TERM
void convert(const char* __indexable str) {
    const char* __null_terminated convert = __unsafe_null_terminated_from_indexable(str, /*terminator=*/ &str[1]);
}
#else
void convert(int* __indexable buf) {
    int* __terminated_by(-1) convert = __unsafe_terminated_by_from_indexable(-1, buf, /*terminator=*/ &buf[1]);
}
#endif

// We effectively need to walk the IR backwards starting from the debug info
// and then locating the expected IR instructions. We don't have a good way
// of walking the file backwards other than make everything a CHECK-DAG.
// That would allow many different orderings rather than the one we want, which
// isn't ideal. Instead make the input to FileCheck be two copies of the IR so
// that we can first match the debug info and then the IR instructions.

// In first copy of the file

// CHECK-DAG: [[OPT_REMARK_0:![0-9]+]] = !{!"bounds-safety-check-terminated-by-from-indexable-ptr-le-term-ptr"}
// BASIC-DAG: [[TRAP_SCOPE_0:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$Terminator pointer below bounds", scope: [[FILE_SCOPE_0:![0-9]+]], file: [[FILE_SCOPE_0]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// DETAILED-NULL-DAG: [[TRAP_SCOPE_0:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$converting 'const char *__indexable' to 'const char *__null_terminated' where pointer '&str[1]' to the null terminator is below the lower bound of 'str'", scope: [[FILE_SCOPE_0:![0-9]+]], file: [[FILE_SCOPE_0]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// DETAILED-NON-NULL-DAG: [[TRAP_SCOPE_0:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$converting 'int *__indexable' to 'int *__terminated_by(-1)' where pointer '&buf[1]' to the terminator '-1' is below the lower bound of 'buf'", scope: [[FILE_SCOPE_0:![0-9]+]], file: [[FILE_SCOPE_0]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// CHECK-DAG: [[TRAP_LOC_0:![0-9]+]] = !DILocation(line: 0, scope: [[TRAP_SCOPE_0]], inlinedAt: [[SRC_LOC_0:![0-9]+]])

// CHECK-DAG: [[OPT_REMARK_1:![0-9]+]] = !{!"bounds-safety-check-terminated-by-from-indexable-term-ptr-no-overflow"}
// BASIC-DAG: [[TRAP_SCOPE_1:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$Terminator pointer overflows address space", scope: [[FILE_SCOPE_1:![0-9]+]], file: [[FILE_SCOPE_1]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// DETAILED-NULL-DAG: [[TRAP_SCOPE_1:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$converting 'const char *__indexable' to 'const char *__null_terminated' where pointer '&str[1]' to the null terminator overflows the address space", scope: [[FILE_SCOPE_1:![0-9]+]], file: [[FILE_SCOPE_1]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// DETAILED-NON-NULL-DAG: [[TRAP_SCOPE_1:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$converting 'int *__indexable' to 'int *__terminated_by(-1)' where pointer '&buf[1]' to the terminator '-1' overflows the address space", scope: [[FILE_SCOPE_1:![0-9]+]], file: [[FILE_SCOPE_1]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// CHECK-DAG: [[TRAP_LOC_1:![0-9]+]] = !DILocation(line: 0, scope: [[TRAP_SCOPE_1]], inlinedAt: [[SRC_LOC_1:![0-9]+]])

// CHECK-DAG: [[OPT_REMARK_2:![0-9]+]] = !{!"bounds-safety-check-terminated-by-from-indexable-term-ptr-plus-one-le-upper-bound"}
// BASIC-DAG: [[TRAP_SCOPE_2:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$Terminator pointer above bounds", scope: [[FILE_SCOPE_2:![0-9]+]], file: [[FILE_SCOPE_2]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// DETAILED-NULL-DAG: [[TRAP_SCOPE_2:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$converting 'const char *__indexable' to 'const char *__null_terminated' where pointer '&str[1]' to the null terminator is above the upper bound of 'str'", scope: [[FILE_SCOPE_2:![0-9]+]], file: [[FILE_SCOPE_2]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// DETAILED-NON-NULL-DAG: [[TRAP_SCOPE_2:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$converting 'int *__indexable' to 'int *__terminated_by(-1)' where pointer '&buf[1]' to the terminator '-1' is above the upper bound of 'buf'", scope: [[FILE_SCOPE_2:![0-9]+]], file: [[FILE_SCOPE_2]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// CHECK-DAG: [[TRAP_LOC_2:![0-9]+]] = !DILocation(line: 0, scope: [[TRAP_SCOPE_2]], inlinedAt: [[SRC_LOC_2:![0-9]+]])

// CHECK-LABEL: ; __SEPERATOR__
// In second copy of the file
// CHECK-LABEL: @convert
// Note: We use the `OPT_REMARK` to make sure we match against the correct branch.
// Unfortunately `SRC_LOC` is not enough because it isn't unique.

// CHECK: br i1 %{{.+}}, label %{{.+}}, label %[[TRAP_LABEL_0:[a-z0-9]+]], !dbg [[SRC_LOC_0]], !prof ![[PROFILE_METADATA:[0-9]+]], !annotation [[OPT_REMARK_0]]
// CHECK: [[TRAP_LABEL_0]]:
// CHECK-NEXT: call void @llvm.ubsantrap(i8 25) {{.*}}!dbg [[TRAP_LOC_0]]

// CHECK: br i1 %{{.+}}, label %{{.+}}, label %[[TRAP_LABEL_1:[a-z0-9]+]], !dbg [[SRC_LOC_1]], !prof ![[PROFILE_METADATA]], !annotation [[OPT_REMARK_1]]
// CHECK: [[TRAP_LABEL_1]]:
// CHECK-NEXT: call void @llvm.ubsantrap(i8 25) {{.*}}!dbg [[TRAP_LOC_1]]

// CHECK: br i1 %{{.+}}, label %{{.+}}, label %[[TRAP_LABEL_2:[a-z0-9]+]], !dbg [[SRC_LOC_2]], !prof ![[PROFILE_METADATA]], !annotation [[OPT_REMARK_2]]
// CHECK: [[TRAP_LABEL_2]]:
// CHECK-NEXT: call void @llvm.ubsantrap(i8 25) {{.*}}!dbg [[TRAP_LOC_2]]
