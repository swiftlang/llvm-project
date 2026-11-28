// Test the trap reasons for the checks performed when dereferencing a pointer
// to a struct with a flexible array member.

// RUN: echo "; __SEPERATOR__" > %t.sep

// RUN: %clang_cc1 -O0 -debug-info-kind=standalone -dwarf-version=5 \
// RUN:   -fbounds-safety -fbounds-safety-debug-trap-reasons=basic \
// RUN:   -emit-llvm %s -o %t.ll
// RUN: cat %t.ll %t.sep %t.ll > %t.repeated.ll
// RUN: FileCheck %s --check-prefixes=CHECK,BASIC --input-file=%t.repeated.ll

// RUN: %clang_cc1 -O0 -debug-info-kind=standalone -dwarf-version=5 \
// RUN:   -fbounds-safety -fbounds-safety-debug-trap-reasons=detailed \
// RUN:   -emit-llvm %s -o %t2.ll
// RUN: cat %t2.ll %t.sep %t2.ll > %t2.repeated.ll
// RUN: FileCheck %s --check-prefixes=CHECK,DETAILED --input-file=%t2.repeated.ll

#include <ptrcheck.h>

struct flexible {
    int count;
    int elems[__counted_by(count)];
};

int deref(struct flexible *__bidi_indexable flex, int i) {
    return (*flex).elems[i];
}

// We effectively need to walk the IR backwards starting from the debug info
// and then locating the expected IR instructions. We don't have a good way
// of walking the file backwards other than make everything a CHECK-DAG.
// That would allow many different orderings rather than the one we want, which
// isn't ideal. Instead make the input to FileCheck be two copies of the IR so
// that we can first match the debug info and then the IR instructions.
//
// All the checks have the same source location. It is captured from the first
// check because other checks in the function share some of the legacy trap
// messages but have a different source location.

// In first copy of the file
// CHECK-DAG: [[OPT_REMARK_OVERFLOW:![0-9]+]] = !{!"bounds-safety-check-one-past-end-overflow"}
// BASIC-DAG: [[TRAP_SCOPE_OVERFLOW:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$Pointer to one past the end overflows address space", scope: [[FILE_SCOPE:![0-9]+]], file: [[FILE_SCOPE:![0-9]+]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// DETAILED-DAG: [[TRAP_SCOPE_OVERFLOW:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$dereferencing 'struct flexible *__bidi_indexable' where pointer 'flex' overflows the address space", scope: [[FILE_SCOPE:![0-9]+]], file: [[FILE_SCOPE:![0-9]+]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// CHECK-DAG: [[TRAP_LOC_OVERFLOW:![0-9]+]] = !DILocation(line: 0, scope: [[TRAP_SCOPE_OVERFLOW]], inlinedAt: [[SRC_LOC:![0-9]+]])

// CHECK-DAG: [[OPT_REMARK_NEGATIVE:![0-9]+]] = !{!"bounds-safety-check-count-negative"}
// BASIC-DAG: [[TRAP_SCOPE_NEGATIVE:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$Count is negative", scope: [[FILE_SCOPE:![0-9]+]], file: [[FILE_SCOPE:![0-9]+]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// DETAILED-DAG: [[TRAP_SCOPE_NEGATIVE:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$dereferencing 'struct flexible *__bidi_indexable' where count 'flex->count' for flexible array member 'elems' is negative", scope: [[FILE_SCOPE:![0-9]+]], file: [[FILE_SCOPE:![0-9]+]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// CHECK-DAG: [[TRAP_LOC_NEGATIVE:![0-9]+]] = !DILocation(line: 0, scope: [[TRAP_SCOPE_NEGATIVE]], inlinedAt: [[SRC_LOC]])

// CHECK-DAG: [[OPT_REMARK_UPPER:![0-9]+]] = !{!"bounds-safety-check-flexible-count-gt-bounds", !"bounds-safety-check-ptr-le-upper-bound"}
// BASIC-DAG: [[TRAP_SCOPE_UPPER:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$Dereferencing above bounds", scope: [[FILE_SCOPE:![0-9]+]], file: [[FILE_SCOPE:![0-9]+]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// DETAILED-DAG: [[TRAP_SCOPE_UPPER:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$dereferencing 'struct flexible *__bidi_indexable' where flexible array member 'elems' starts above the upper bound of pointer 'flex'", scope: [[FILE_SCOPE:![0-9]+]], file: [[FILE_SCOPE:![0-9]+]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// CHECK-DAG: [[TRAP_LOC_UPPER:![0-9]+]] = !DILocation(line: 0, scope: [[TRAP_SCOPE_UPPER]], inlinedAt: [[SRC_LOC]])

// CHECK-DAG: [[OPT_REMARK_LOWER:![0-9]+]] = !{!"bounds-safety-check-flexible-count-gt-bounds", !"bounds-safety-check-ptr-ge-lower-bound"}
// BASIC-DAG: [[TRAP_SCOPE_LOWER:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$Dereferencing below bounds", scope: [[FILE_SCOPE:![0-9]+]], file: [[FILE_SCOPE:![0-9]+]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// DETAILED-DAG: [[TRAP_SCOPE_LOWER:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$dereferencing 'struct flexible *__bidi_indexable' where pointer 'flex' is below its lower bound", scope: [[FILE_SCOPE:![0-9]+]], file: [[FILE_SCOPE:![0-9]+]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// CHECK-DAG: [[TRAP_LOC_LOWER:![0-9]+]] = !DILocation(line: 0, scope: [[TRAP_SCOPE_LOWER]], inlinedAt: [[SRC_LOC]])

// CHECK-DAG: [[OPT_REMARK_TOO_BIG:![0-9]+]] = !{!"bounds-safety-check-flexible-count-gt-bounds"}
// BASIC-DAG: [[TRAP_SCOPE_TOO_BIG:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$Count for flexible array member is too big", scope: [[FILE_SCOPE:![0-9]+]], file: [[FILE_SCOPE:![0-9]+]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// DETAILED-DAG: [[TRAP_SCOPE_TOO_BIG:![0-9]+]] = distinct !DISubprogram(name: "__clang_trap_msg$Bounds check failed$dereferencing 'struct flexible *__bidi_indexable' where count 'flex->count' for flexible array member 'elems' exceeds the bounds of pointer 'flex'", scope: [[FILE_SCOPE:![0-9]+]], file: [[FILE_SCOPE:![0-9]+]], type: {{![0-9]+}}, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: {{![0-9]+}}
// CHECK-DAG: [[TRAP_LOC_TOO_BIG:![0-9]+]] = !DILocation(line: 0, scope: [[TRAP_SCOPE_TOO_BIG]], inlinedAt: [[SRC_LOC]])

// CHECK-LABEL: ; __SEPERATOR__

// In second copy of the file
// CHECK-LABEL: @deref
// Note: We use the `OPT_REMARK` to make sure we match against the correct branch.
// CHECK: br i1 %{{.+}}, label %{{.+}}, label %[[TRAP_LABEL_OVERFLOW:[a-z0-9]+]], !dbg [[SRC_LOC]], !prof ![[PROFILE_METADATA:[0-9]+]], !annotation [[OPT_REMARK_OVERFLOW]]
// CHECK: [[TRAP_LABEL_OVERFLOW]]:
// CHECK-NEXT: call void @llvm.ubsantrap(i8 25) {{.*}}!dbg [[TRAP_LOC_OVERFLOW]]
// CHECK: br i1 %{{.+}}, label %{{.+}}, label %[[TRAP_LABEL_NEGATIVE:[a-z0-9]+]], !dbg [[SRC_LOC]], !prof ![[PROFILE_METADATA]], !annotation [[OPT_REMARK_NEGATIVE]]
// CHECK: [[TRAP_LABEL_NEGATIVE]]:
// CHECK-NEXT: call void @llvm.ubsantrap(i8 25) {{.*}}!dbg [[TRAP_LOC_NEGATIVE]]
// CHECK: br i1 %{{.+}}, label %{{.+}}, label %[[TRAP_LABEL_UPPER:[a-z0-9]+]], !dbg [[SRC_LOC]], !prof ![[PROFILE_METADATA]], !annotation [[OPT_REMARK_UPPER]]
// CHECK: [[TRAP_LABEL_UPPER]]:
// CHECK-NEXT: call void @llvm.ubsantrap(i8 25) {{.*}}!dbg [[TRAP_LOC_UPPER]]
// CHECK: br i1 %{{.+}}, label %{{.+}}, label %[[TRAP_LABEL_LOWER:[a-z0-9]+]], !dbg [[SRC_LOC]], !prof ![[PROFILE_METADATA]], !annotation [[OPT_REMARK_LOWER]]
// CHECK: [[TRAP_LABEL_LOWER]]:
// CHECK-NEXT: call void @llvm.ubsantrap(i8 25) {{.*}}!dbg [[TRAP_LOC_LOWER]]
// CHECK: br i1 %{{.+}}, label %{{.+}}, label %[[TRAP_LABEL_TOO_BIG:[a-z0-9]+]], !dbg [[SRC_LOC]], !prof ![[PROFILE_METADATA]], !annotation [[OPT_REMARK_TOO_BIG]]
// CHECK: [[TRAP_LABEL_TOO_BIG]]:
// CHECK-NEXT: call void @llvm.ubsantrap(i8 25) {{.*}}!dbg [[TRAP_LOC_TOO_BIG]]
