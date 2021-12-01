// REQUIRES: x86
// RUN: rm -rf %t; split-file %s %t
// RUN: mkdir -p %t/alib %t/dlib

// RUN: llvm-mc -filetype=obj -triple=x86_64-apple-macos %t/main.s -o %t/main.o
// RUN: llvm-mc -filetype=obj -triple=x86_64-apple-macos %t/t1.s -o %t/t1.o
// RUN: llvm-mc -filetype=obj -triple=x86_64-apple-macos %t/t2.s -o %t/t2.o
// RUN: llvm-ar rcs %t/alib/libt1.a %t/t1.o
// RUN: %lld -dylib -o %t/dlib/libt2.dylib %t/t2.o

// Check normal invocation.
// RUN: %lld %t/main.o -o %t/exe-normal -lt1 -lt2 -L%t/alib -L%t/dlib
// RUN: llvm-objdump --macho %t/exe-normal -t | FileCheck %s -check-prefix=SYMBOLS

// Check result caching.

// RUN: %lld --fcas-builtin-path %t/cas --fcas-cache-results --verbose %t/main.o -o %t/exe -lt1 -lt2 -L%t/alib -L%t/dlib 2>&1 \
// RUN:    | FileCheck %s -check-prefix=CACHE-MISS
// RUN: diff %t/exe %t/exe-normal

// RUN: rm %t/exe
// RUN: %lld --fcas-builtin-path %t/cas --fcas-cache-results --verbose %t/main.o -o %t/exe -lt1 -lt2 -L%t/alib -L%t/dlib 2>&1 \
// RUN:    | FileCheck %s -check-prefix=CACHE-HIT
// RUN: diff %t/exe %t/exe-normal

// RUN: touch %t/main.o %t/alib/libt1.a %t/dlib/libt2.dylib
// RUN: %lld --fcas-builtin-path %t/cas --fcas-cache-results --verbose %t/main.o -o %t/exe -lt1 -lt2 -L%t/alib -L%t/dlib 2>&1 \
// RUN:    | FileCheck %s -check-prefix=CACHE-HIT

// Check that changing output executable filename doesn't affect cache key for executables.
// RUN: %lld --fcas-builtin-path %t/cas --fcas-cache-results --verbose %t/main.o -o %t/exe2 -lt1 -lt2 -L%t/alib -L%t/dlib 2>&1 \
// RUN:    | FileCheck %s -check-prefix=CACHE-HIT
// RUN: diff %t/exe2 %t/exe-normal

// But changing output dylib filename affects cache key.
// RUN: %lld --fcas-builtin-path %t/cas --fcas-cache-results --verbose -dylib %t/main.o -o %t/exe1.dylib -lt1 -lt2 -L%t/alib -L%t/dlib 2>&1 \
// RUN:    | FileCheck %s -check-prefix=CACHE-MISS
// RUN: %lld --fcas-builtin-path %t/cas --fcas-cache-results --verbose -dylib %t/main.o -o %t/exe2.dylib -lt1 -lt2 -L%t/alib -L%t/dlib 2>&1 \
// RUN:    | FileCheck %s -check-prefix=CACHE-MISS
// RUN: not diff %t/exe1.dylib %t/exe2.dylib

// Check that re-exports in a dylib are handled for dependency scanning (libc++ re-exports libc++abi)
// RUN: %lld --fcas-builtin-path %t/cas --fcas-cache-results --verbose -lc++ %t/main.o -o %t/exe -lt1 -lt2 -L%t/alib -L%t/dlib 2>&1 \
// RUN:    | FileCheck %s -check-prefix=CACHE-MISS

// Change order of search paths.
// RUN: %lld --fcas-builtin-path %t/cas --fcas-cache-results --verbose %t/main.o -o %t/exe -lt2 -lt1 -L%t/alib -L%t/dlib 2>&1 \
// RUN:    | FileCheck %s -check-prefix=CACHE-MISS
// RUN: %lld --fcas-builtin-path %t/cas --fcas-cache-results --verbose %t/main.o -o %t/exe -lt2 -lt1 -L%t/alib -L%t/dlib 2>&1 \
// RUN:    | FileCheck %s -check-prefix=CACHE-HIT

// Change .o contents
// RUN: llvm-mc -filetype=obj -triple=x86_64-apple-macos %t/main2.s -o %t/main.o
// RUN: %lld --fcas-builtin-path %t/cas --fcas-cache-results --verbose %t/main.o -o %t/exe -lt1 -lt2 -L%t/alib -L%t/dlib 2>&1 \
// RUN:    | FileCheck %s -check-prefix=CACHE-MISS
// RUN: %lld --fcas-builtin-path %t/cas --fcas-cache-results --verbose %t/main.o -o %t/exe -lt1 -lt2 -L%t/alib -L%t/dlib 2>&1 \
// RUN:    | FileCheck %s -check-prefix=CACHE-HIT

// Change .a and .dylib contents
// RUN: llvm-ar rcs %t/alib/libt1.a %t/t2.o
// RUN: %lld -dylib -o %t/dlib/libt2.dylib %t/t1.o
// RUN: %lld --fcas-builtin-path %t/cas --fcas-cache-results --verbose %t/main.o -o %t/exe -lt1 -lt2 -L%t/alib -L%t/dlib 2>&1 \
// RUN:    | FileCheck %s -check-prefix=CACHE-MISS
// RUN: %lld --fcas-builtin-path %t/cas --fcas-cache-results --verbose %t/main.o -o %t/exe -lt1 -lt2 -L%t/alib -L%t/dlib 2>&1 \
// RUN:    | FileCheck %s -check-prefix=CACHE-HIT

// Check using relative paths.

// RUN: mv %t/exe %t/exe.bak; cd %t
// RUN: %lld --fcas-builtin-path %t/cas --fcas-cache-results --verbose main.o -o exe -lt1 -lt2 -Lalib -Ldlib 2>&1 \
// RUN:    | FileCheck %s -check-prefix=CACHE-HIT
// RUN: diff %t/exe %t/exe.bak

// CACHE-MISS:      Caching: cache miss
// CACHE-MISS-NEXT: Caching: cached result

// CACHE-HIT: Caching: cache hit

// SYMBOLS: F __TEXT,__text _main
// SYMBOLS: F __TEXT,__text _call1
// SYMBOLS: *UND* _putchar
// SYMBOLS: *UND* _call2

//--- main.s
	.linker_option "-lSystem"
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

//--- main2.s
	.linker_option "-lSystem"
	.section	__TEXT,__text,regular,pure_instructions
	.globl	_main
	.p2align	4, 0x90
_main:
	pushq	%rax
	movl	$0, 4(%rsp)
	callq	_call1
	callq	_call2
	movl	$11, %edi
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
