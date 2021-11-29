// REQUIRES: x86
// RUN: rm -rf %t; split-file %s %t
// RUN: mkdir -p %t/alib %t/dlib

// RUN: llvm-mc -filetype=obj -triple=x86_64-apple-macos %t/main.s -o %t/main.o
// RUN: llvm-mc -filetype=obj -triple=x86_64-apple-macos %t/t1.s -o %t/t1.o
// RUN: llvm-mc -filetype=obj -triple=x86_64-apple-macos %t/t2.s -o %t/t2.o
// RUN: llvm-ar rcs %t/alib/libt1.a %t/t1.o
// RUN: %lld -dylib -o %t/dlib/libt2.dylib %t/t2.o

// Check with normal filesystem access.
// RUN: %lld -lSystem %t/main.o -o %t/exe1 -lt1 -lt2 -L%t/alib -L%t/dlib
// RUN: llvm-objdump --macho %t/exe1 -t | FileCheck %s -check-prefix=SYMBOLS

// Check with CAS filesystem.

// RUN: llvm-cas -cas %t/cas --merge > %t/empty.id
// RUN: not %lld --fcas-builtin-path %t/cas --fcas-fs @%t/empty.id -lSystem %t/main.o -o %t/exe2 -lt1 -lt2 -L%t/alib -L%t/dlib 2>&1 \
// RUN:    | FileCheck %s -check-prefix=FAIL-DIR -check-prefix=FAIL-SYSLIB -check-prefix=FAIL-MAIN -check-prefix=FAIL-USERLIB

// RUN: llvm-cas -cas %t/cas --merge %t/main.o > %t/main.o.id
// RUN: not %lld --fcas-builtin-path %t/cas --fcas-fs @%t/main.o.id -lSystem %t/main.o -o %t/exe2 -lt1 -lt2 -L%t/alib -L%t/dlib 2>&1 \
// RUN:    | FileCheck %s -check-prefix=FAIL-DIR -check-prefix=FAIL-SYSLIB -check-prefix=FAIL-USERLIB -check-prefix=FAIL-SYSSYM -check-prefix=FAIL-USERSYM

// RUN: llvm-cas -cas %t/cas --merge %t/main.o %t/alib %t/dlib > %t/o-and-libs.id
// RUN: not %lld --fcas-builtin-path %t/cas --fcas-fs @%t/o-and-libs.id -lSystem %t/main.o -o %t/exe2 -lt1 -lt2 -L%t/alib -L%t/dlib 2>&1 \
// RUN:    | FileCheck %s -check-prefix=FAIL-SYSLIB -check-prefix=FAIL-SYSSYM

// RUN: llvm-cas -cas %t/cas --merge @%t/o-and-libs.id %S/../Inputs/MacOSX.sdk > %t/complete.id
// RUN: %lld --fcas-builtin-path %t/cas --fcas-fs @%t/complete.id -lSystem %t/main.o -o %t/exe2 -lt1 -lt2 -L%t/alib -L%t/dlib
// RUN: llvm-objdump --macho %t/exe2 -t | FileCheck %s -check-prefix=SYMBOLS

// FAIL-DIR: error: directory not found for option -L{{.*}}alib
// FAIL-DIR: error: directory not found for option -L{{.*}}dlib
// FAIL-SYSLIB: error: library not found for -lSystem
// FAIL-MAIN: error: cannot open {{.*}}main.o: No such file or directory
// FAIL-USERLIB: error: library not found for -lt1
// FAIL-USERLIB: error: library not found for -lt2

// FAIL-SYSSYM: error: undefined symbol: _putchar
// FAIL-USERSYM: error: undefined symbol: _call2
// FAIL-USERSYM: error: undefined symbol: _call1

// SYMBOLS: F __TEXT,__text _main
// SYMBOLS: F __TEXT,__text _call1
// SYMBOLS: *UND* _putchar
// SYMBOLS: *UND* _call2

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
