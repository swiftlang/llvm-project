// REQUIRES: x86
// RUN: rm -rf %t; mkdir -p %t
// RUN: llvm-mc -filetype=obj -triple=x86_64-apple-macos %s -o %t/test.o
// RUN: llvm-cas-object-format %t/test.o -cas %t/cas -ingest-schema=flatv1 -silent > %t/casid-flat.txt
// RUN: llvm-cas-object-format %t/test.o -cas %t/cas -ingest-schema=nestedv1 -silent > %t/casid-nested.txt
// RUN: %lld -lc++ -lSystem -cas_path %t/cas @%t/casid-flat.txt -o %t/test-flat
// RUN: %lld -lc++ -lSystem -cas_path %t/cas @%t/casid-nested.txt -o %t/test-nested
// RUN: llvm-objdump --macho %t/test-flat --full-contents -d -t | \
// RUN:     FileCheck %s --check-prefix=CHECK --check-prefix=FLAT
// RUN: llvm-objdump --macho %t/test-nested --full-contents -d -t | \
// RUN:     FileCheck %s --check-prefix=CHECK --check-prefix=NESTED

// NESTED: __GLOBAL__sub_I_test.cpp:
// NESTED: callq ___cxx_global_var_init

// CHECK: ___cxx_global_var_init:
// CHECK: leaq	_mys1(%rip), %rdi
// CHECK: callq	__ZN3MySC1Ev ## MyS::MyS()
// CHECK: leaq	__ZN3MySD1Ev(%rip), %rdi ## MyS::~MyS()
// CHECK: leaq	_mys1(%rip), %rsi
// CHECK: leaq	__mh_execute_header(%rip), %rdx
// CHECK: callq	{{.*}} ## symbol stub for: ___cxa_atexit

// FLAT: __GLOBAL__sub_I_test.cpp:
// FLAT: callq ___cxx_global_var_init

// CHECK: Contents of section __TEXT,__stubs:
// CHECK: Contents of section __TEXT,__stub_helper:
// CHECK: Contents of section __DATA_CONST,__got:
// CHECK: Contents of section __DATA_CONST,__mod_init_func:
// CHECK: Contents of section __DATA,__la_symbol_ptr:
// CHECK: Contents of section __DATA,__data:
// CHECK: Contents of section __DATA,__common:

// CHECK: SYMBOL TABLE:

// FLAT: l     F __TEXT,__text ___cxx_global_var_init
// FLAT: l     F __TEXT,__text __GLOBAL__sub_I_test.cpp
// FLAT: l     O __DATA,__data __dyld_private
// FLAT: l     F __TEXT,__text __ZN3MySC1Ev
// FLAT: l     F __TEXT,__text __ZN3MySD1Ev
// FLAT: l     F __TEXT,__text __ZN3MySC2Ev
// FLAT: l     F __TEXT,__text __ZN3MySD2Ev

// NESTED: l     F __TEXT,__text __GLOBAL__sub_I_test.cpp
// NESTED: l     F __TEXT,__text ___cxx_global_var_init
// NESTED: l     O __DATA,__data __dyld_private
// NESTED: l     F __TEXT,__text __ZN3MySC1Ev
// NESTED: l     F __TEXT,__text __ZN3MySC2Ev
// NESTED: l     F __TEXT,__text __ZN3MySD2Ev
// NESTED: l     F __TEXT,__text __ZN3MySD1Ev

// CHECK: g     F __TEXT,__text _main
// CHECK: g     O __DATA,__common _mys1
// CHECK: g     F __TEXT,__text __mh_execute_header
// CHECK:         *UND* ___cxa_atexit
// CHECK:         *UND* dyld_stub_binder

## This is assembly output of:
##
##  struct MyS {
##    char c[7];
##    MyS() {}
##    ~MyS() {}
##  };
##  MyS mys1;
##  int main() {
##    return 0;
##  }
##
	.section	__TEXT,__text,regular,pure_instructions
	.section	__TEXT,__StaticInit,regular,pure_instructions
	.p2align	4, 0x90                         ## -- Begin function __cxx_global_var_init
___cxx_global_var_init:                 ## @__cxx_global_var_init
## %bb.0:                               ## %entry
	pushq	%rax
	leaq	_mys1(%rip), %rdi
	callq	__ZN3MySC1Ev
	movq	__ZN3MySD1Ev@GOTPCREL(%rip), %rdi
	leaq	_mys1(%rip), %rsi
	leaq	___dso_handle(%rip), %rdx
	callq	___cxa_atexit
	popq	%rax
	retq
                                        ## -- End function
	.section	__TEXT,__text,regular,pure_instructions
	.globl	__ZN3MySC1Ev                    ## -- Begin function _ZN3MySC1Ev
	.weak_def_can_be_hidden	__ZN3MySC1Ev
	.p2align	4, 0x90
__ZN3MySC1Ev:                           ## @_ZN3MySC1Ev
## %bb.0:                               ## %entry
	pushq	%rax
	movq	%rdi, (%rsp)
	movq	(%rsp), %rdi
	callq	__ZN3MySC2Ev
	popq	%rax
	retq
                                        ## -- End function
	.globl	__ZN3MySD1Ev                    ## -- Begin function _ZN3MySD1Ev
	.weak_def_can_be_hidden	__ZN3MySD1Ev
	.p2align	4, 0x90
__ZN3MySD1Ev:                           ## @_ZN3MySD1Ev
## %bb.0:                               ## %entry
	pushq	%rax
	movq	%rdi, (%rsp)
	movq	(%rsp), %rdi
	callq	__ZN3MySD2Ev
	popq	%rax
	retq
                                        ## -- End function
	.globl	_main                           ## -- Begin function main
	.p2align	4, 0x90
_main:                                  ## @main
## %bb.0:                               ## %entry
	movl	$0, -4(%rsp)
	xorl	%eax, %eax
	retq
                                        ## -- End function
	.globl	__ZN3MySC2Ev                    ## -- Begin function _ZN3MySC2Ev
	.weak_def_can_be_hidden	__ZN3MySC2Ev
	.p2align	4, 0x90
__ZN3MySC2Ev:                           ## @_ZN3MySC2Ev
## %bb.0:                               ## %entry
	movq	%rdi, -8(%rsp)
	retq
                                        ## -- End function
	.globl	__ZN3MySD2Ev                    ## -- Begin function _ZN3MySD2Ev
	.weak_def_can_be_hidden	__ZN3MySD2Ev
	.p2align	4, 0x90
__ZN3MySD2Ev:                           ## @_ZN3MySD2Ev
## %bb.0:                               ## %entry
	movq	%rdi, -8(%rsp)
	retq
                                        ## -- End function
	.section	__TEXT,__StaticInit,regular,pure_instructions
	.p2align	4, 0x90                         ## -- Begin function _GLOBAL__sub_I_test.cpp
__GLOBAL__sub_I_test.cpp:               ## @_GLOBAL__sub_I_test.cpp
## %bb.0:                               ## %entry
	pushq	%rax
	callq	___cxx_global_var_init
	popq	%rax
	retq
                                        ## -- End function
	.globl	_mys1                           ## @mys1
.zerofill __DATA,__common,_mys1,7,0
	.section	__DATA,__mod_init_func,mod_init_funcs
	.p2align	3
	.quad	__GLOBAL__sub_I_test.cpp
.subsections_via_symbols
