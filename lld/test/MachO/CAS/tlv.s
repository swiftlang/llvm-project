// REQUIRES: x86
// RUN: rm -rf %t; mkdir -p %t
// RUN: llvm-mc -filetype=obj -triple=x86_64-apple-macos %s -o %t/test.o
// RUN: llvm-cas-object-format %t/test.o -cas %t/cas -ingest-schema=flatv1 -silent > %t/casid-flat.txt
// RUN: llvm-cas-object-format %t/test.o -cas %t/cas -ingest-schema=nestedv1 -silent > %t/casid-nested.txt
// RUN: %lld -lSystem -cas_path %t/cas @%t/casid-flat.txt -o %t/test-flat
// RUN: %lld -lSystem -cas_path %t/cas @%t/casid-nested.txt -o %t/test-nested
// RUN: llvm-objdump --macho %t/test-flat --full-contents -d -t | FileCheck %s
// RUN: llvm-objdump --macho %t/test-nested --full-contents -d -t | FileCheck %s

// CHECK: _main:
// CHECK: pushq %rax
// CHECK: movl  $0, 4(%rsp)
// CHECK: leaq  _sometlv(%rip), %rdi
// CHECK: callq *(%rdi)
// CHECK: movl  $1, (%rax)
// CHECK: movl  (%rax), %eax
// CHECK: popq  %rcx
// CHECK: retq

// CHECK: Contents of section __TEXT,__text:
// CHECK: Contents of section __DATA,__thread_vars:
// CHECK: Contents of section __DATA,__thread_bss:
// CHECK:  100001018 cffaedfe

// CHECK: SYMBOL TABLE:
// CHECK: 0000000100001018 l     O __DATA,__thread_bss _sometlv$tlv$init
// CHECK: 00000001000003b0 g     F __TEXT,__text _main
// CHECK: 0000000100001000 g     O __DATA,__thread_vars _sometlv
// CHECK: 0000000100000000 g     F __TEXT,__text __mh_execute_header
// CHECK: 0000000000000000         *UND* dyld_stub_binder
// CHECK: 0000000000000000         *UND* __tlv_bootstrap

## This is assembly output of:
##
## __thread int sometlv;
## int main() {
##   sometlv = 1;
##   return sometlv;
## }
##
	.section	__TEXT,__text,regular,pure_instructions
	.globl	_main                           ## -- Begin function main
	.p2align	4, 0x90
_main:                                  ## @main
## %bb.0:                               ## %entry
	pushq	%rax
	movl	$0, 4(%rsp)
	movq	_sometlv@TLVP(%rip), %rdi
	callq	*(%rdi)
	movl	$1, (%rax)
	movl	(%rax), %eax
	popq	%rcx
	retq
                                        ## -- End function
.tbss _sometlv$tlv$init, 4, 2           ## @sometlv

	.section	__DATA,__thread_vars,thread_local_variables
	.globl	_sometlv
_sometlv:
	.quad	__tlv_bootstrap
	.quad	0
	.quad	_sometlv$tlv$init

.subsections_via_symbols
