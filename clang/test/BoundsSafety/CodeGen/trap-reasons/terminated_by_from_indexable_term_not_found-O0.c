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
    const char* __null_terminated convert = __unsafe_null_terminated_from_indexable(str);
}
#else
void convert(int* __indexable buf) {
    int* __terminated_by(-1) convert = __unsafe_terminated_by_from_indexable(-1, buf);
}
#endif

// We effectively need to walk the IR backwards starting from the debug info
// and then locating the expected IR instructions. We don't have a good way
// of walking the file backwards other than make everything a CHECK-DAG.
// That would allow many different orderings rather than the one we want, which
// isn't ideal. Instead make the input to FileCheck be two copies of the IR so
// that we can first match the debug info and then the IR instructions.

// In first copy of the file
// CHECK-DAG: [[OPT_REMARK:![0-9]+]] = !{!"bounds-safety-check-terminated-by-from-indexable-term"}
// BASIC-DAG: [[TRAP_SCOPE:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$Cannot find the terminator when converting to __terminated_by pointer from an __indexable pointer", scope: [[FILE_SCOPE:![0-9]+]], file: [[FILE_SCOPE:![0-9]+]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// DETAILED-NULL-DAG: [[TRAP_SCOPE:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$converting 'const char *__indexable' to 'const char *__null_terminated' where there is no null terminator within the bounds of 'str'", scope: [[FILE_SCOPE:![0-9]+]], file: [[FILE_SCOPE:![0-9]+]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// DETAILED-NON-NULL-DAG: [[TRAP_SCOPE:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$converting 'int *__indexable' to 'int *__terminated_by(-1)' where there is no terminator '-1' within the bounds of 'buf'", scope: [[FILE_SCOPE:![0-9]+]], file: [[FILE_SCOPE:![0-9]+]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// CHECK-DAG: [[TRAP_LOC:![0-9]+]] = !DILocation(line: 0, scope: [[TRAP_SCOPE]], inlinedAt: [[SRC_LOC:![0-9]+]])
// CHECK-LABEL: ; __SEPERATOR__

// In second copy of the file
// CHECK-LABEL: @convert
// Note: We use the `OPT_REMARK` to make sure we match against the correct branch.
// Unfortunately `SRC_LOC` is not enough because it isn't unique.
// CHECK: br i1 %{{.+}}, label %{{.+}}, label %[[TRAP_LABEL:[a-z0-9]+]], !dbg [[SRC_LOC]], !prof ![[PROFILE_METADATA:[0-9]+]], !annotation [[OPT_REMARK]]
// CHECK: [[TRAP_LABEL]]:
// CHECK-NEXT: call void @llvm.ubsantrap(i8 25) {{.*}}!dbg [[TRAP_LOC]]
