// REQUIRES: x86
// RUN: rm -rf %t; split-file %s %t
// RUN: mkdir -p %t/alib %t/alib.id

// RUN: llvm-mc -filetype=obj -triple=x86_64-apple-macos %t/main.s -o %t/main.o
// RUN: llvm-mc -filetype=obj -triple=x86_64-apple-macos %t/t1.s -o %t/t1.o
// RUN: llvm-mc -filetype=obj -triple=x86_64-apple-macos %t/t2.s -o %t/t2.o
// RUN: llvm-libtool-darwin -static -o %t/alib/libt.a %t/t1.o %t/t2.o

// Check normal invocation.
// RUN: %lld -lSystem -framework CoreFoundation %t/main.o -o %t/exe-normal -lt -L%t/alib
// RUN: llvm-objdump --macho %t/exe-normal -t | FileCheck %s -check-prefix=SYMBOLS

// Check with embedded CASIDS.
// RUN: llvm-cas-object-format -cas %t/cas -just-blobs %t/main.o -casid-output %t/main.id.o
// RUN: %lld --fcas-builtin-path %t/cas -lSystem -framework CoreFoundation %t/main.id.o -o %t/exe -lt -L%t/alib
// RUN: diff %t/exe %t/exe-normal

// RUN: llvm-cas-object-format -cas %t/cas -just-blobs %t/t1.o -casid-output %t/t1.id.o
// RUN: llvm-cas-object-format -cas %t/cas -just-blobs %t/t2.o -casid-output %t/t2.id.o
// RUN: llvm-libtool-darwin -fcas builtin -fcas-builtin-path %t/cas -static -o %t/alib.id/libt.a %t/t1.id.o %t/t2.id.o
// RUN: %lld --fcas-builtin-path %t/cas -lSystem -framework CoreFoundation %t/main.id.o -o %t/exe -lt -L%t/alib.id
// RUN: diff %t/exe %t/exe-normal

// Check with CAS schema.
// RUN: llvm-cas-object-format -cas %t/cas -ingest-schema=flatv1 %t/main.o -casid-output %t/main.schema.o
// RUN: %lld --fcas-builtin-path %t/cas -lSystem -framework CoreFoundation %t/main.schema.o -o %t/exe-schema -lt -L%t/alib.id
// RUN: llvm-objdump --macho %t/exe-schema -t | FileCheck %s -check-prefix=SYMBOLS

// Check with cached result
// RUN: %lld --fcas-builtin-path %t/cas --fcas-cache-results --verbose -lSystem -framework CoreFoundation %t/main.id.o -o %t/exe.1 -lt -L%t/alib.id 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CACHE-MISS
// RUN: %lld --fcas-builtin-path %t/cas --fcas-cache-results --verbose -lSystem -framework CoreFoundation %t/main.id.o -o %t/exe.2 -lt -L%t/alib.id 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CACHE-HIT
// RUN: diff %t/exe.1 %t/exe.2

// SYMBOLS-DAG: F __TEXT,__text _main
// SYMBOLS-DAG: F __TEXT,__text _call1
// SYMBOLS-DAG: F __TEXT,__text _call2
// SYMBOLS-DAG: *UND* _putchar
// SYMBOLS-DAG: *UND* ___CFConstantStringClassReference

// CACHE-MISS:      Caching: cache miss
// CACHE-MISS-NEXT: Caching: cached result

// CACHE-HIT: Caching: cache hit

//--- main.s
	.section	__TEXT,__text,regular,pure_instructions
	.globl	_main
	.p2align	4, 0x90
_main:
	pushq	%rax
	movl	$0, 4(%rsp)
	callq	_call1
	callq	_call2
	movl	$10, %edi
	callq	_putchar
	xorl	%eax, %eax
	popq	%rcx
	retq

.subsections_via_symbols

//--- t1.s
	.section	__TEXT,__text,regular,pure_instructions
	.globl	_call1
	.p2align	4, 0x90
_call1:
	retq

.subsections_via_symbols

//--- t2.s
	.section	__TEXT,__text,regular,pure_instructions
	.globl	_call2
	.p2align	4, 0x90
_call2:
	retq

.subsections_via_symbols

	.section	__TEXT,__text,regular,pure_instructions
	.globl	_dummy
	.p2align	4, 0x90
_dummy:
  callq ___CFConstantStringClassReference
	retq

.subsections_via_symbols
