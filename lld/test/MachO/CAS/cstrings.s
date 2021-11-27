// REQUIRES: x86
// RUN: rm -rf %t; mkdir -p %t
// RUN: llvm-mc -filetype=obj -triple=x86_64-apple-macos %s -o %t/test.o
// RUN: llvm-cas-object-format %t/test.o -cas %t/cas -ingest-schema=flatv1 -silent > %t/casid-flat.txt
// RUN: llvm-cas-object-format %t/test.o -cas %t/cas -ingest-schema=nestedv1 -silent > %t/casid-nested.txt
// RUN: %lld --fcas-builtin-path %t/cas @%t/casid-flat.txt -o %t/test-flat
// RUN: %lld --fcas-builtin-path %t/cas @%t/casid-nested.txt -o %t/test-nested
// RUN: llvm-objdump --macho %t/test-flat --full-contents -d -t | FileCheck %s
// RUN: llvm-objdump --macho %t/test-nested --full-contents -d -t | FileCheck %s

// CHECK: _main:
// CHECK: movq	_svar1(%rip), %rdi
// CHECK: callq	_puts_stub
// CHECK: movq	_svar2(%rip), %rdi
// CHECK: callq	_puts_stub
// CHECK: movq	_svar3(%rip), %rdi
// CHECK: callq	_puts_stub

// CHECK: _puts_stub:
// CHECK: movq	%rdi, -8(%rsp)

// CHECK: Contents of section __TEXT,__cstring:
// CHECK:  1000003d0 68656c6c 6f00776f 726c6400 2100      hello.world.!.
// CHECK: Contents of section __DATA,__data:
// CHECK:  100001000 d0030000 01000000 d6030000 01000000
// CHECK:  100001010 dc030000 01000000

// CHECK: SYMBOL TABLE:
// CHECK: 00000001000003c0 l     F __TEXT,__text _puts_stub
// CHECK: 0000000100000380 g     F __TEXT,__text _main
// CHECK: 0000000100001000 g     O __DATA,__data _svar1
// CHECK: 0000000100001008 g     O __DATA,__data _svar2
// CHECK: 0000000100001010 g     O __DATA,__data _svar3


## This is assembly output of:
##
##  const char *svar1 = "hello";
##  const char *svar2 = "world";
##  const char *svar3 = "!";
##  static inline void puts_stub(const char *);
##  int main() {
##    puts_stub(svar1);
##    puts_stub(svar2);
##    puts_stub(svar3);
##    return 0;
##  }
##  static inline void puts_stub(const char *) {}
##
	.section	__TEXT,__text,regular,pure_instructions
	.globl	_main                           ## -- Begin function main
	.p2align	4, 0x90
_main:                                  ## @main
## %bb.0:                               ## %entry
	pushq	%rax
	movl	$0, 4(%rsp)
	movq	_svar1(%rip), %rdi
	callq	_puts_stub
	movq	_svar2(%rip), %rdi
	callq	_puts_stub
	movq	_svar3(%rip), %rdi
	callq	_puts_stub
	xorl	%eax, %eax
	popq	%rcx
	retq
                                        ## -- End function
	.p2align	4, 0x90                         ## -- Begin function puts_stub
_puts_stub:                             ## @puts_stub
## %bb.0:                               ## %entry
	movq	%rdi, -8(%rsp)
	retq
                                        ## -- End function
	.section	__TEXT,__cstring,cstring_literals
L_.str:                                 ## @.str
	.asciz	"hello"

	.section	__DATA,__data
	.globl	_svar1                          ## @svar1
	.p2align	3
_svar1:
	.quad	L_.str

	.section	__TEXT,__cstring,cstring_literals
L_.str.1:                               ## @.str.1
	.asciz	"world"

	.section	__DATA,__data
	.globl	_svar2                          ## @svar2
	.p2align	3
_svar2:
	.quad	L_.str.1

	.section	__TEXT,__cstring,cstring_literals
L_.str.2:                               ## @.str.2
	.asciz	"!"

	.section	__DATA,__data
	.globl	_svar3                          ## @svar3
	.p2align	3
_svar3:
	.quad	L_.str.2

.subsections_via_symbols
