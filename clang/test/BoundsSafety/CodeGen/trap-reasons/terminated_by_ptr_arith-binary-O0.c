// =============================================================================
// Null terminator
// =============================================================================

// RUN: %clang_cc1 -O0 -debug-info-kind=standalone -dwarf-version=5 \
// RUN:   -fbounds-safety -fbounds-safety-debug-trap-reasons=basic \
// RUN:   -emit-llvm %s -o - | FileCheck --check-prefixes=CHECK,BASIC,NULL %s

// RUN: %clang_cc1 -O0 -debug-info-kind=standalone -dwarf-version=5 \
// RUN:   -fbounds-safety -fbounds-safety-debug-trap-reasons=detailed \
// RUN:   -emit-llvm %s -o - | FileCheck --check-prefixes=CHECK,DETAILED-NULL,NULL %s

// =============================================================================
// Non-null terminator
// =============================================================================

// RUN: %clang_cc1 -O0 -debug-info-kind=standalone -dwarf-version=5 \
// RUN:   -fbounds-safety -fbounds-safety-debug-trap-reasons=basic \
// RUN:   -emit-llvm -DNON_NULL_TERM %s -o - | \
// RUN:   FileCheck --check-prefixes=CHECK,BASIC,NON-NULL %s

// RUN: %clang_cc1 -O0 -debug-info-kind=standalone -dwarf-version=5 \
// RUN:   -fbounds-safety -fbounds-safety-debug-trap-reasons=detailed \
// RUN:   -emit-llvm -DNON_NULL_TERM %s -o - | \
// RUN:   FileCheck --check-prefixes=CHECK,DETAILED-NON-NULL,NON-NULL %s

#include <ptrcheck.h>

#ifndef NON_NULL_TERM
const char* __null_terminated next(const char* __null_terminated str) {
    return str + 1;
}
#else
int* __terminated_by(-1) next(int* __terminated_by(-1) buf) {
    return buf + 1;
}
#endif

// CHECK-LABEL: @next

// NULL: %[[CMP_REG:[a-z0-9]+]] = icmp ne i8 %{{.+}}, 0, !dbg [[LOC:![0-9]+]]
// NON-NULL: %[[CMP_REG:[a-z0-9]+]] = icmp ne i32 %{{.+}}, -1, !dbg [[LOC:![0-9]+]]
// CHECK-NEXT: br i1 %[[CMP_REG]], label %{{.+}}, label %[[TRAP_LABEL:[a-z0-9]+]], !dbg [[LOC]]

// CHECK: [[TRAP_LABEL]]:
// CHECK-NEXT: call void @llvm.ubsantrap(i8 25) {{.*}}!dbg [[TRAP_LOC:![0-9]+]]

// CHECK-DAG: [[TRAP_LOC]] = !DILocation(line: 0, scope: [[TRAP_SCOPE:![0-9]+]], inlinedAt: [[LOC]])
// BASIC-DAG: [[TRAP_SCOPE]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$Arithmetic on __terminated_by pointer one-past-the-end of the terminator", scope: [[FILE_SCOPE:![0-9]+]], file: [[FILE_SCOPE:![0-9]+]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// DETAILED-NULL-DAG: [[TRAP_SCOPE]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$incrementing 'const char *__null_terminated' pointer past the null terminator in 'str + 1'", scope: [[FILE_SCOPE:![0-9]+]], file: [[FILE_SCOPE:![0-9]+]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// DETAILED-NON-NULL-DAG: [[TRAP_SCOPE]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$incrementing 'int *__terminated_by(-1)' pointer past the terminator '-1' in 'buf + 1'", scope: [[FILE_SCOPE:![0-9]+]], file: [[FILE_SCOPE:![0-9]+]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
