// REQUIRES: x86-registered-target
// RUN: llvm-mc -triple x86_64-apple-macos11 %s -filetype=obj -o %t.o
// RUN: llvm-cas-object-format --cas %t/cas --ingest-schema=flatv1 %t.o
// RUN: llvm-cas-object-format --cas %t/cas --ingest-schema=nestedv1 %t.o

.text
.p2align  6
_test:
test:
  ret
