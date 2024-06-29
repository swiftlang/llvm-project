// RUN: rm -rf %t && mkdir %t

// RUN: c-index-test core -cas-store -cas-path %t/cas -cas-object-data aGVsbG8sIHdvcmxkIQo= 1> %t/obj1.txt

// RUN: c-index-test core -cas-load -cas-path %t/cas @%t/obj1.txt 1> %t/out1.txt
// RUN: FileCheck %s -input-file %t/out1.txt -check-prefix=OBJECTONE
//
// OBJECTONE: Data:
// OBJECTONE: aGVsbG8sIHdvcmxkIQo=

// RUN: c-index-test core -cas-store -cas-path %t/cas -cas-object-data Zm9vCg== --cas-object-ref @%t/obj1.txt 1> %t/obj2.txt

// RUN: c-index-test core -cas-load -cas-path %t/cas @%t/obj2.txt 1> %t/out2.txt
// RUN: FileCheck %s -input-file %t/out2.txt -check-prefix=OBJECTTWO
//
// OBJECTTWO: Refs:
// OBJECTTWO: llvmcas://{{[a-z0-9]+}}
// OBJECTTWO: Data:
// OBJECTTWO: Zm9vCg==