	.text
	.file	"neon-dotreduce.ll"
	.globl	test_udot_v4i8                  // -- Begin function test_udot_v4i8
	.p2align	2
	.type	test_udot_v4i8,@function
test_udot_v4i8:                         // @test_udot_v4i8
	.cfi_startproc
// %bb.0:                               // %entry
	ldr	s0, [x1]
	ldr	s1, [x0]
	ushll	v0.8h, v0.8b, #0
	ushll	v1.8h, v1.8b, #0
	umull	v0.4s, v0.4h, v1.4h
	addv	s0, v0.4s
	fmov	w8, s0
	add	w0, w8, w2
	ret
.Lfunc_end0:
	.size	test_udot_v4i8, .Lfunc_end0-test_udot_v4i8
	.cfi_endproc
                                        // -- End function
	.globl	test_udot_v4i8_nomla            // -- Begin function test_udot_v4i8_nomla
	.p2align	2
	.type	test_udot_v4i8_nomla,@function
test_udot_v4i8_nomla:                   // @test_udot_v4i8_nomla
	.cfi_startproc
// %bb.0:                               // %entry
	ldr	s0, [x0]
	ushll	v0.8h, v0.8b, #0
	ushll	v0.4s, v0.4h, #0
	addv	s0, v0.4s
	fmov	w0, s0
	ret
.Lfunc_end1:
	.size	test_udot_v4i8_nomla, .Lfunc_end1-test_udot_v4i8_nomla
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v4i8                  // -- Begin function test_sdot_v4i8
	.p2align	2
	.type	test_sdot_v4i8,@function
test_sdot_v4i8:                         // @test_sdot_v4i8
	.cfi_startproc
// %bb.0:                               // %entry
	ldr	s0, [x1]
	ldr	s1, [x0]
	sshll	v0.8h, v0.8b, #0
	sshll	v1.8h, v1.8b, #0
	smull	v0.4s, v0.4h, v1.4h
	addv	s0, v0.4s
	fmov	w8, s0
	add	w0, w8, w2
	ret
.Lfunc_end2:
	.size	test_sdot_v4i8, .Lfunc_end2-test_sdot_v4i8
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v4i8_double           // -- Begin function test_sdot_v4i8_double
	.p2align	2
	.type	test_sdot_v4i8_double,@function
test_sdot_v4i8_double:                  // @test_sdot_v4i8_double
	.cfi_startproc
// %bb.0:                               // %entry
	ushll	v3.4s, v3.4h, #0
	ushll	v2.4s, v2.4h, #0
	shl	v3.4s, v3.4s, #24
	shl	v2.4s, v2.4s, #24
	ushll	v1.4s, v1.4h, #0
	sshr	v3.4s, v3.4s, #24
	ushll	v0.4s, v0.4h, #0
	sshr	v2.4s, v2.4s, #24
	shl	v0.4s, v0.4s, #24
	shl	v1.4s, v1.4s, #24
	mul	v2.4s, v2.4s, v3.4s
	sshr	v0.4s, v0.4s, #24
	sshr	v1.4s, v1.4s, #24
	mla	v2.4s, v0.4s, v1.4s
	addv	s0, v2.4s
	fmov	w0, s0
	ret
.Lfunc_end3:
	.size	test_sdot_v4i8_double, .Lfunc_end3-test_sdot_v4i8_double
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v4i8_double_nomla     // -- Begin function test_sdot_v4i8_double_nomla
	.p2align	2
	.type	test_sdot_v4i8_double_nomla,@function
test_sdot_v4i8_double_nomla:            // @test_sdot_v4i8_double_nomla
	.cfi_startproc
// %bb.0:                               // %entry
	ushll	v0.4s, v0.4h, #0
	ushll	v1.4s, v2.4h, #0
	shl	v0.4s, v0.4s, #24
	shl	v1.4s, v1.4s, #24
	sshr	v0.4s, v0.4s, #24
	ssra	v0.4s, v1.4s, #24
	addv	s0, v0.4s
	fmov	w0, s0
	ret
.Lfunc_end4:
	.size	test_sdot_v4i8_double_nomla, .Lfunc_end4-test_sdot_v4i8_double_nomla
	.cfi_endproc
                                        // -- End function
	.globl	test_udot_v5i8                  // -- Begin function test_udot_v5i8
	.p2align	2
	.type	test_udot_v5i8,@function
test_udot_v5i8:                         // @test_udot_v5i8
	.cfi_startproc
// %bb.0:                               // %entry
	ldr	d0, [x1]
	ldr	d1, [x0]
	ushll	v0.8h, v0.8b, #0
	ushll	v1.8h, v1.8b, #0
	umull2	v2.4s, v0.8h, v1.8h
	mov	v2.s[1], wzr
	mov	v2.s[2], wzr
	mov	v2.s[3], wzr
	umlal	v2.4s, v0.4h, v1.4h
	addv	s0, v2.4s
	fmov	w8, s0
	add	w0, w8, w2
	ret
.Lfunc_end5:
	.size	test_udot_v5i8, .Lfunc_end5-test_udot_v5i8
	.cfi_endproc
                                        // -- End function
	.globl	test_udot_v5i8_nomla            // -- Begin function test_udot_v5i8_nomla
	.p2align	2
	.type	test_udot_v5i8_nomla,@function
test_udot_v5i8_nomla:                   // @test_udot_v5i8_nomla
	.cfi_startproc
// %bb.0:                               // %entry
	ldr	d0, [x0]
	ushll	v0.8h, v0.8b, #0
	ushll2	v1.4s, v0.8h, #0
	mov	v1.s[1], wzr
	mov	v1.s[2], wzr
	mov	v1.s[3], wzr
	uaddw	v0.4s, v1.4s, v0.4h
	addv	s0, v0.4s
	fmov	w0, s0
	ret
.Lfunc_end6:
	.size	test_udot_v5i8_nomla, .Lfunc_end6-test_udot_v5i8_nomla
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v5i8                  // -- Begin function test_sdot_v5i8
	.p2align	2
	.type	test_sdot_v5i8,@function
test_sdot_v5i8:                         // @test_sdot_v5i8
	.cfi_startproc
// %bb.0:                               // %entry
	ldr	d0, [x1]
	ldr	d1, [x0]
	sshll	v0.8h, v0.8b, #0
	sshll	v1.8h, v1.8b, #0
	smull2	v2.4s, v0.8h, v1.8h
	mov	v2.s[1], wzr
	mov	v2.s[2], wzr
	mov	v2.s[3], wzr
	smlal	v2.4s, v0.4h, v1.4h
	addv	s0, v2.4s
	fmov	w8, s0
	add	w0, w8, w2
	ret
.Lfunc_end7:
	.size	test_sdot_v5i8, .Lfunc_end7-test_sdot_v5i8
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v5i8_double           // -- Begin function test_sdot_v5i8_double
	.p2align	2
	.type	test_sdot_v5i8_double,@function
test_sdot_v5i8_double:                  // @test_sdot_v5i8_double
	.cfi_startproc
// %bb.0:                               // %entry
	sshll	v2.8h, v2.8b, #0
	sshll	v0.8h, v0.8b, #0
	sshll	v1.8h, v1.8b, #0
	sshll	v3.8h, v3.8b, #0
	smull2	v4.4s, v0.8h, v1.8h
	smull2	v5.4s, v2.8h, v3.8h
	mov	v4.s[1], wzr
	mov	v5.s[1], wzr
	mov	v4.s[2], wzr
	mov	v5.s[2], wzr
	mov	v4.s[3], wzr
	mov	v5.s[3], wzr
	smlal	v4.4s, v0.4h, v1.4h
	smlal	v5.4s, v2.4h, v3.4h
	add	v0.4s, v4.4s, v5.4s
	addv	s0, v0.4s
	fmov	w0, s0
	ret
.Lfunc_end8:
	.size	test_sdot_v5i8_double, .Lfunc_end8-test_sdot_v5i8_double
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v5i8_double_nomla     // -- Begin function test_sdot_v5i8_double_nomla
	.p2align	2
	.type	test_sdot_v5i8_double_nomla,@function
test_sdot_v5i8_double_nomla:            // @test_sdot_v5i8_double_nomla
	.cfi_startproc
// %bb.0:                               // %entry
	sshll	v0.8h, v0.8b, #0
	sshll	v1.8h, v2.8b, #0
	sshll2	v2.4s, v0.8h, #0
	sshll2	v3.4s, v1.8h, #0
	mov	v2.s[1], wzr
	mov	v3.s[1], wzr
	mov	v2.s[2], wzr
	mov	v3.s[2], wzr
	mov	v2.s[3], wzr
	mov	v3.s[3], wzr
	saddw	v0.4s, v2.4s, v0.4h
	saddw	v1.4s, v3.4s, v1.4h
	add	v0.4s, v0.4s, v1.4s
	addv	s0, v0.4s
	fmov	w0, s0
	ret
.Lfunc_end9:
	.size	test_sdot_v5i8_double_nomla, .Lfunc_end9-test_sdot_v5i8_double_nomla
	.cfi_endproc
                                        // -- End function
	.globl	test_udot_v8i8                  // -- Begin function test_udot_v8i8
	.p2align	2
	.type	test_udot_v8i8,@function
test_udot_v8i8:                         // @test_udot_v8i8
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v0.2d, #0000000000000000
	ldr	d1, [x0]
	ldr	d2, [x1]
	udot	v0.2s, v2.8b, v1.8b
	addp	v0.2s, v0.2s, v0.2s
	fmov	w0, s0
	ret
.Lfunc_end10:
	.size	test_udot_v8i8, .Lfunc_end10-test_udot_v8i8
	.cfi_endproc
                                        // -- End function
	.globl	test_udot_v8i8_nomla            // -- Begin function test_udot_v8i8_nomla
	.p2align	2
	.type	test_udot_v8i8_nomla,@function
test_udot_v8i8_nomla:                   // @test_udot_v8i8_nomla
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v0.8b, #1
	ldr	d2, [x0]
	movi	v1.2d, #0000000000000000
	udot	v1.2s, v2.8b, v0.8b
	addp	v0.2s, v1.2s, v1.2s
	fmov	w0, s0
	ret
.Lfunc_end11:
	.size	test_udot_v8i8_nomla, .Lfunc_end11-test_udot_v8i8_nomla
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v8i8                  // -- Begin function test_sdot_v8i8
	.p2align	2
	.type	test_sdot_v8i8,@function
test_sdot_v8i8:                         // @test_sdot_v8i8
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v0.2d, #0000000000000000
	ldr	d1, [x0]
	ldr	d2, [x1]
	sdot	v0.2s, v2.8b, v1.8b
	addp	v0.2s, v0.2s, v0.2s
	fmov	w0, s0
	ret
.Lfunc_end12:
	.size	test_sdot_v8i8, .Lfunc_end12-test_sdot_v8i8
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v8i8_nomla            // -- Begin function test_sdot_v8i8_nomla
	.p2align	2
	.type	test_sdot_v8i8_nomla,@function
test_sdot_v8i8_nomla:                   // @test_sdot_v8i8_nomla
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v0.8b, #1
	ldr	d2, [x0]
	movi	v1.2d, #0000000000000000
	sdot	v1.2s, v2.8b, v0.8b
	addp	v0.2s, v1.2s, v1.2s
	fmov	w0, s0
	ret
.Lfunc_end13:
	.size	test_sdot_v8i8_nomla, .Lfunc_end13-test_sdot_v8i8_nomla
	.cfi_endproc
                                        // -- End function
	.globl	test_udot_v16i8                 // -- Begin function test_udot_v16i8
	.p2align	2
	.type	test_udot_v16i8,@function
test_udot_v16i8:                        // @test_udot_v16i8
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v0.2d, #0000000000000000
	ldr	q1, [x1]
	ldr	q2, [x0]
	udot	v0.4s, v1.16b, v2.16b
	addv	s0, v0.4s
	fmov	w8, s0
	add	w0, w8, w2
	ret
.Lfunc_end14:
	.size	test_udot_v16i8, .Lfunc_end14-test_udot_v16i8
	.cfi_endproc
                                        // -- End function
	.globl	test_udot_v16i8_nomla           // -- Begin function test_udot_v16i8_nomla
	.p2align	2
	.type	test_udot_v16i8_nomla,@function
test_udot_v16i8_nomla:                  // @test_udot_v16i8_nomla
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v0.16b, #1
	ldr	q2, [x0]
	movi	v1.2d, #0000000000000000
	udot	v1.4s, v2.16b, v0.16b
	addv	s0, v1.4s
	fmov	w0, s0
	ret
.Lfunc_end15:
	.size	test_udot_v16i8_nomla, .Lfunc_end15-test_udot_v16i8_nomla
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v16i8                 // -- Begin function test_sdot_v16i8
	.p2align	2
	.type	test_sdot_v16i8,@function
test_sdot_v16i8:                        // @test_sdot_v16i8
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v0.2d, #0000000000000000
	ldr	q1, [x1]
	ldr	q2, [x0]
	sdot	v0.4s, v1.16b, v2.16b
	addv	s0, v0.4s
	fmov	w8, s0
	add	w0, w8, w2
	ret
.Lfunc_end16:
	.size	test_sdot_v16i8, .Lfunc_end16-test_sdot_v16i8
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v16i8_nomla           // -- Begin function test_sdot_v16i8_nomla
	.p2align	2
	.type	test_sdot_v16i8_nomla,@function
test_sdot_v16i8_nomla:                  // @test_sdot_v16i8_nomla
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v0.16b, #1
	ldr	q2, [x0]
	movi	v1.2d, #0000000000000000
	sdot	v1.4s, v2.16b, v0.16b
	addv	s0, v1.4s
	fmov	w0, s0
	ret
.Lfunc_end17:
	.size	test_sdot_v16i8_nomla, .Lfunc_end17-test_sdot_v16i8_nomla
	.cfi_endproc
                                        // -- End function
	.globl	test_udot_v8i8_double           // -- Begin function test_udot_v8i8_double
	.p2align	2
	.type	test_udot_v8i8_double,@function
test_udot_v8i8_double:                  // @test_udot_v8i8_double
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v4.2d, #0000000000000000
	udot	v4.2s, v2.8b, v3.8b
	udot	v4.2s, v0.8b, v1.8b
	addp	v0.2s, v4.2s, v4.2s
	fmov	w0, s0
	ret
.Lfunc_end18:
	.size	test_udot_v8i8_double, .Lfunc_end18-test_udot_v8i8_double
	.cfi_endproc
                                        // -- End function
	.globl	test_udot_v8i8_double_nomla     // -- Begin function test_udot_v8i8_double_nomla
	.p2align	2
	.type	test_udot_v8i8_double_nomla,@function
test_udot_v8i8_double_nomla:            // @test_udot_v8i8_double_nomla
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v1.8b, #1
	movi	v3.2d, #0000000000000000
	udot	v3.2s, v2.8b, v1.8b
	udot	v3.2s, v0.8b, v1.8b
	addp	v0.2s, v3.2s, v3.2s
	fmov	w0, s0
	ret
.Lfunc_end19:
	.size	test_udot_v8i8_double_nomla, .Lfunc_end19-test_udot_v8i8_double_nomla
	.cfi_endproc
                                        // -- End function
	.globl	test_udot_v16i8_double          // -- Begin function test_udot_v16i8_double
	.p2align	2
	.type	test_udot_v16i8_double,@function
test_udot_v16i8_double:                 // @test_udot_v16i8_double
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v4.2d, #0000000000000000
	udot	v4.4s, v2.16b, v3.16b
	udot	v4.4s, v0.16b, v1.16b
	addv	s0, v4.4s
	fmov	w0, s0
	ret
.Lfunc_end20:
	.size	test_udot_v16i8_double, .Lfunc_end20-test_udot_v16i8_double
	.cfi_endproc
                                        // -- End function
	.globl	test_udot_v16i8_double_nomla    // -- Begin function test_udot_v16i8_double_nomla
	.p2align	2
	.type	test_udot_v16i8_double_nomla,@function
test_udot_v16i8_double_nomla:           // @test_udot_v16i8_double_nomla
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v1.16b, #1
	movi	v3.2d, #0000000000000000
	udot	v3.4s, v2.16b, v1.16b
	udot	v3.4s, v0.16b, v1.16b
	addv	s0, v3.4s
	fmov	w0, s0
	ret
.Lfunc_end21:
	.size	test_udot_v16i8_double_nomla, .Lfunc_end21-test_udot_v16i8_double_nomla
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v8i8_double           // -- Begin function test_sdot_v8i8_double
	.p2align	2
	.type	test_sdot_v8i8_double,@function
test_sdot_v8i8_double:                  // @test_sdot_v8i8_double
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v4.2d, #0000000000000000
	sdot	v4.2s, v2.8b, v3.8b
	sdot	v4.2s, v0.8b, v1.8b
	addp	v0.2s, v4.2s, v4.2s
	fmov	w0, s0
	ret
.Lfunc_end22:
	.size	test_sdot_v8i8_double, .Lfunc_end22-test_sdot_v8i8_double
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v8i8_double_nomla     // -- Begin function test_sdot_v8i8_double_nomla
	.p2align	2
	.type	test_sdot_v8i8_double_nomla,@function
test_sdot_v8i8_double_nomla:            // @test_sdot_v8i8_double_nomla
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v1.8b, #1
	movi	v3.2d, #0000000000000000
	sdot	v3.2s, v2.8b, v1.8b
	sdot	v3.2s, v0.8b, v1.8b
	addp	v0.2s, v3.2s, v3.2s
	fmov	w0, s0
	ret
.Lfunc_end23:
	.size	test_sdot_v8i8_double_nomla, .Lfunc_end23-test_sdot_v8i8_double_nomla
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v16i8_double          // -- Begin function test_sdot_v16i8_double
	.p2align	2
	.type	test_sdot_v16i8_double,@function
test_sdot_v16i8_double:                 // @test_sdot_v16i8_double
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v4.2d, #0000000000000000
	sdot	v4.4s, v2.16b, v3.16b
	sdot	v4.4s, v0.16b, v1.16b
	addv	s0, v4.4s
	fmov	w0, s0
	ret
.Lfunc_end24:
	.size	test_sdot_v16i8_double, .Lfunc_end24-test_sdot_v16i8_double
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v16i8_double_nomla    // -- Begin function test_sdot_v16i8_double_nomla
	.p2align	2
	.type	test_sdot_v16i8_double_nomla,@function
test_sdot_v16i8_double_nomla:           // @test_sdot_v16i8_double_nomla
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v1.16b, #1
	movi	v3.2d, #0000000000000000
	sdot	v3.4s, v2.16b, v1.16b
	sdot	v3.4s, v0.16b, v1.16b
	addv	s0, v3.4s
	fmov	w0, s0
	ret
.Lfunc_end25:
	.size	test_sdot_v16i8_double_nomla, .Lfunc_end25-test_sdot_v16i8_double_nomla
	.cfi_endproc
                                        // -- End function
	.globl	test_udot_v24i8                 // -- Begin function test_udot_v24i8
	.p2align	2
	.type	test_udot_v24i8,@function
test_udot_v24i8:                        // @test_udot_v24i8
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v0.2d, #0000000000000000
	ldr	q1, [x0]
	movi	v3.2d, #0000000000000000
	ldr	d2, [x0, #16]
	ldr	d4, [x1, #16]
	ldr	q5, [x1]
	udot	v0.2s, v4.8b, v2.8b
	udot	v3.4s, v5.16b, v1.16b
	addp	v0.2s, v0.2s, v0.2s
	addv	s1, v3.4s
	fmov	w8, s0
	fmov	w9, s1
	add	w8, w9, w8
	add	w0, w8, w2
	ret
.Lfunc_end26:
	.size	test_udot_v24i8, .Lfunc_end26-test_udot_v24i8
	.cfi_endproc
                                        // -- End function
	.globl	test_udot_v24i8_nomla           // -- Begin function test_udot_v24i8_nomla
	.p2align	2
	.type	test_udot_v24i8_nomla,@function
test_udot_v24i8_nomla:                  // @test_udot_v24i8_nomla
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v0.8b, #1
	ldr	d4, [x0, #16]
	movi	v1.2d, #0000000000000000
	ldr	q5, [x0]
	movi	v2.2d, #0000000000000000
	movi	v3.16b, #1
	udot	v2.2s, v4.8b, v0.8b
	udot	v1.4s, v5.16b, v3.16b
	addp	v0.2s, v2.2s, v2.2s
	addv	s1, v1.4s
	fmov	w8, s0
	fmov	w9, s1
	add	w0, w9, w8
	ret
.Lfunc_end27:
	.size	test_udot_v24i8_nomla, .Lfunc_end27-test_udot_v24i8_nomla
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v24i8                 // -- Begin function test_sdot_v24i8
	.p2align	2
	.type	test_sdot_v24i8,@function
test_sdot_v24i8:                        // @test_sdot_v24i8
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v0.2d, #0000000000000000
	ldr	q1, [x0]
	movi	v3.2d, #0000000000000000
	ldr	d2, [x0, #16]
	ldr	d4, [x1, #16]
	ldr	q5, [x1]
	sdot	v0.2s, v4.8b, v2.8b
	sdot	v3.4s, v5.16b, v1.16b
	addp	v0.2s, v0.2s, v0.2s
	addv	s1, v3.4s
	fmov	w8, s0
	fmov	w9, s1
	add	w8, w9, w8
	add	w0, w8, w2
	ret
.Lfunc_end28:
	.size	test_sdot_v24i8, .Lfunc_end28-test_sdot_v24i8
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v24i8_double          // -- Begin function test_sdot_v24i8_double
	.p2align	2
	.type	test_sdot_v24i8_double,@function
test_sdot_v24i8_double:                 // @test_sdot_v24i8_double
	.cfi_startproc
// %bb.0:                               // %entry
	ldr	b0, [sp, #128]
	add	x8, sp, #136
	fmov	s1, w0
	add	x9, sp, #152
	ldr	b2, [sp, #256]
	add	x11, sp, #264
	ld1	{ v0.b }[1], [x8]
	add	x8, sp, #144
	mov	v1.b[1], w1
	add	x10, sp, #168
	ld1	{ v2.b }[1], [x11]
	add	x11, sp, #520
	ldr	b4, [sp, #512]
	add	x12, sp, #72
	ld1	{ v0.b }[2], [x8]
	add	x8, sp, #160
	mov	v1.b[2], w2
	ldr	b5, [sp, #320]
	ld1	{ v4.b }[1], [x11]
	add	x11, sp, #328
	ldr	b3, [sp, #64]
	add	x13, sp, #80
	ld1	{ v0.b }[3], [x9]
	add	x9, sp, #176
	mov	v1.b[3], w3
	ld1	{ v5.b }[1], [x11]
	ld1	{ v3.b }[1], [x12]
	add	x12, sp, #184
	add	x11, sp, #528
	ldr	b7, [sp, #640]
	ld1	{ v0.b }[4], [x8]
	add	x8, sp, #272
	mov	v1.b[4], w4
	ldr	b16, [sp, #448]
	ld1	{ v3.b }[2], [x13]
	ld1	{ v2.b }[2], [x8]
	add	x8, sp, #336
	ld1	{ v0.b }[5], [x10]
	add	x10, sp, #280
	mov	v1.b[5], w5
	ld1	{ v4.b }[2], [x11]
	ld1	{ v5.b }[2], [x8]
	add	x8, sp, #344
	ld1	{ v2.b }[3], [x10]
	add	x10, sp, #192
	ld1	{ v0.b }[6], [x9]
	add	x9, sp, #88
	mov	v1.b[6], w6
	add	x11, sp, #536
	ld1	{ v5.b }[3], [x8]
	mov	x8, sp
	ld1	{ v3.b }[3], [x9]
	add	x9, sp, #288
	ld1	{ v0.b }[7], [x12]
	mov	v1.b[7], w7
	ld1	{ v4.b }[3], [x11]
	add	x11, sp, #96
	ld1	{ v2.b }[4], [x9]
	add	x9, sp, #544
	ld1	{ v0.b }[8], [x10]
	add	x10, sp, #200
	ld1	{ v1.b }[8], [x8]
	add	x8, sp, #8
	ld1	{ v3.b }[4], [x11]
	add	x11, sp, #352
	ld1	{ v4.b }[4], [x9]
	add	x9, sp, #296
	ld1	{ v0.b }[9], [x10]
	add	x10, sp, #208
	ld1	{ v1.b }[9], [x8]
	add	x8, sp, #16
	ld1	{ v5.b }[4], [x11]
	add	x11, sp, #104
	ld1	{ v2.b }[5], [x9]
	add	x9, sp, #552
	ld1	{ v0.b }[10], [x10]
	add	x10, sp, #216
	ld1	{ v3.b }[5], [x11]
	add	x11, sp, #360
	ld1	{ v1.b }[10], [x8]
	add	x8, sp, #24
	ld1	{ v4.b }[5], [x9]
	add	x9, sp, #304
	ld1	{ v0.b }[11], [x10]
	add	x10, sp, #224
	ld1	{ v5.b }[5], [x11]
	add	x11, sp, #112
	ld1	{ v1.b }[11], [x8]
	add	x8, sp, #32
	ld1	{ v2.b }[6], [x9]
	add	x9, sp, #560
	ld1	{ v0.b }[12], [x10]
	add	x10, sp, #232
	ld1	{ v3.b }[6], [x11]
	add	x11, sp, #368
	ld1	{ v1.b }[12], [x8]
	add	x8, sp, #40
	ld1	{ v4.b }[6], [x9]
	add	x9, sp, #312
	ld1	{ v5.b }[6], [x11]
	add	x11, sp, #568
	ld1	{ v0.b }[13], [x10]
	add	x10, sp, #240
	ld1	{ v1.b }[13], [x8]
	add	x8, sp, #48
	ld1	{ v2.b }[7], [x9]
	add	x9, sp, #376
	ld1	{ v4.b }[7], [x11]
	add	x11, sp, #576
	ld1	{ v0.b }[14], [x10]
	add	x10, sp, #248
	ld1	{ v5.b }[7], [x9]
	add	x9, sp, #120
	ld1	{ v1.b }[14], [x8]
	add	x8, sp, #56
	ld1	{ v4.b }[8], [x11]
	add	x11, sp, #384
	ld1	{ v0.b }[15], [x10]
	add	x10, sp, #584
	ld1	{ v3.b }[7], [x9]
	add	x9, sp, #648
	ld1	{ v5.b }[8], [x11]
	add	x11, sp, #592
	ld1	{ v1.b }[15], [x8]
	add	x8, sp, #392
	ld1	{ v4.b }[9], [x10]
	add	x10, sp, #456
	ld1	{ v7.b }[1], [x9]
	add	x9, sp, #656
	ld1	{ v5.b }[9], [x8]
	add	x8, sp, #400
	ld1	{ v16.b }[1], [x10]
	add	x10, sp, #464
	ld1	{ v4.b }[10], [x11]
	add	x11, sp, #600
	ld1	{ v7.b }[2], [x9]
	add	x9, sp, #664
	ld1	{ v5.b }[10], [x8]
	add	x8, sp, #408
	ld1	{ v16.b }[2], [x10]
	add	x10, sp, #472
	ld1	{ v4.b }[11], [x11]
	add	x11, sp, #608
	ld1	{ v7.b }[3], [x9]
	add	x9, sp, #672
	ld1	{ v5.b }[11], [x8]
	add	x8, sp, #416
	ld1	{ v16.b }[3], [x10]
	add	x10, sp, #480
	ld1	{ v4.b }[12], [x11]
	add	x11, sp, #616
	ld1	{ v7.b }[4], [x9]
	add	x9, sp, #680
	ld1	{ v5.b }[12], [x8]
	add	x8, sp, #424
	ld1	{ v16.b }[4], [x10]
	add	x10, sp, #488
	ld1	{ v4.b }[13], [x11]
	add	x11, sp, #624
	ld1	{ v7.b }[5], [x9]
	add	x9, sp, #688
	ld1	{ v5.b }[13], [x8]
	add	x8, sp, #432
	ld1	{ v16.b }[5], [x10]
	add	x10, sp, #496
	ld1	{ v4.b }[14], [x11]
	add	x11, sp, #632
	ld1	{ v7.b }[6], [x9]
	add	x9, sp, #696
	ld1	{ v5.b }[14], [x8]
	add	x8, sp, #440
	ld1	{ v16.b }[6], [x10]
	add	x10, sp, #504
	movi	v6.2d, #0000000000000000
	ld1	{ v4.b }[15], [x11]
	movi	v17.2d, #0000000000000000
	ld1	{ v7.b }[7], [x9]
	ld1	{ v5.b }[15], [x8]
	movi	v18.2d, #0000000000000000
	ld1	{ v16.b }[7], [x10]
	movi	v19.2d, #0000000000000000
	sdot	v17.4s, v1.16b, v0.16b
	sdot	v6.4s, v5.16b, v4.16b
	sdot	v18.2s, v3.8b, v2.8b
	sdot	v19.2s, v16.8b, v7.8b
	addv	s0, v17.4s
	addv	s2, v6.4s
	addp	v1.2s, v18.2s, v18.2s
	addp	v3.2s, v19.2s, v19.2s
	fmov	w8, s0
	fmov	w10, s2
	fmov	w9, s1
	fmov	w11, s3
	add	w8, w8, w9
	add	w9, w10, w11
	add	w0, w8, w9
	ret
.Lfunc_end29:
	.size	test_sdot_v24i8_double, .Lfunc_end29-test_sdot_v24i8_double
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v24i8_double_nomla    // -- Begin function test_sdot_v24i8_double_nomla
	.p2align	2
	.type	test_sdot_v24i8_double_nomla,@function
test_sdot_v24i8_double_nomla:           // @test_sdot_v24i8_double_nomla
	.cfi_startproc
// %bb.0:                               // %entry
	fmov	s0, w0
	ldr	b1, [sp, #320]
	add	x8, sp, #328
	add	x9, sp, #376
	add	x10, sp, #8
	ldr	b2, [sp, #64]
	mov	v0.b[1], w1
	add	x13, sp, #72
	ld1	{ v1.b }[1], [x8]
	add	x8, sp, #336
	ldr	b3, [sp, #448]
	add	x14, sp, #456
	ld1	{ v2.b }[1], [x13]
	add	x11, sp, #16
	mov	v0.b[2], w2
	add	x13, sp, #80
	ld1	{ v1.b }[2], [x8]
	add	x8, sp, #344
	ld1	{ v3.b }[1], [x14]
	add	x14, sp, #464
	ld1	{ v2.b }[2], [x13]
	add	x12, sp, #24
	mov	v0.b[3], w3
	add	x13, sp, #88
	ld1	{ v1.b }[3], [x8]
	add	x8, sp, #352
	ld1	{ v3.b }[2], [x14]
	add	x14, sp, #472
	ld1	{ v2.b }[3], [x13]
	add	x13, sp, #96
	mov	v0.b[4], w4
	movi	v6.8b, #1
	ld1	{ v1.b }[4], [x8]
	add	x8, sp, #360
	ld1	{ v3.b }[3], [x14]
	add	x14, sp, #480
	ld1	{ v2.b }[4], [x13]
	add	x13, sp, #104
	mov	v0.b[5], w5
	ld1	{ v1.b }[5], [x8]
	add	x8, sp, #368
	ld1	{ v3.b }[4], [x14]
	add	x14, sp, #488
	ld1	{ v2.b }[5], [x13]
	add	x13, sp, #496
	mov	v0.b[6], w6
	ld1	{ v1.b }[6], [x8]
	mov	x8, sp
	ld1	{ v3.b }[5], [x14]
	movi	v4.16b, #1
	mov	v0.b[7], w7
	ld1	{ v1.b }[7], [x9]
	add	x9, sp, #384
	ld1	{ v3.b }[6], [x13]
	movi	v5.2d, #0000000000000000
	ld1	{ v0.b }[8], [x8]
	add	x8, sp, #32
	ld1	{ v1.b }[8], [x9]
	add	x9, sp, #392
	movi	v7.2d, #0000000000000000
	movi	v16.2d, #0000000000000000
	ld1	{ v0.b }[9], [x10]
	add	x10, sp, #40
	ld1	{ v1.b }[9], [x9]
	add	x9, sp, #400
	movi	v17.2d, #0000000000000000
	ld1	{ v0.b }[10], [x11]
	add	x11, sp, #48
	ld1	{ v1.b }[10], [x9]
	add	x9, sp, #408
	ld1	{ v0.b }[11], [x12]
	add	x12, sp, #56
	ld1	{ v1.b }[11], [x9]
	add	x9, sp, #416
	ld1	{ v0.b }[12], [x8]
	add	x8, sp, #424
	ld1	{ v1.b }[12], [x9]
	add	x9, sp, #112
	ld1	{ v0.b }[13], [x10]
	add	x10, sp, #120
	ld1	{ v1.b }[13], [x8]
	add	x8, sp, #432
	ld1	{ v2.b }[6], [x9]
	add	x9, sp, #504
	ld1	{ v0.b }[14], [x11]
	ld1	{ v1.b }[14], [x8]
	add	x8, sp, #440
	ld1	{ v2.b }[7], [x10]
	ld1	{ v3.b }[7], [x9]
	ld1	{ v0.b }[15], [x12]
	ld1	{ v1.b }[15], [x8]
	sdot	v7.2s, v2.8b, v6.8b
	sdot	v5.2s, v3.8b, v6.8b
	sdot	v16.4s, v0.16b, v4.16b
	sdot	v17.4s, v1.16b, v4.16b
	addp	v0.2s, v7.2s, v7.2s
	addp	v1.2s, v5.2s, v5.2s
	addv	s2, v16.4s
	addv	s3, v17.4s
	fmov	w8, s0
	fmov	w9, s2
	fmov	w11, s1
	fmov	w10, s3
	add	w8, w9, w8
	add	w9, w10, w11
	add	w0, w8, w9
	ret
.Lfunc_end30:
	.size	test_sdot_v24i8_double_nomla, .Lfunc_end30-test_sdot_v24i8_double_nomla
	.cfi_endproc
                                        // -- End function
	.globl	test_udot_v25i8                 // -- Begin function test_udot_v25i8
	.p2align	2
	.type	test_udot_v25i8,@function
test_udot_v25i8:                        // @test_udot_v25i8
	.cfi_startproc
// %bb.0:                               // %entry
	ldp	q3, q0, [x1]
	ushll	v6.8h, v3.8b, #0
	ushll2	v3.8h, v3.16b, #0
	ldp	q2, q1, [x0]
	ushll2	v5.8h, v0.16b, #0
	ushll	v0.8h, v0.8b, #0
	ushll2	v4.8h, v1.16b, #0
	ushll	v1.8h, v1.8b, #0
	umull	v4.4s, v5.4h, v4.4h
	ushll2	v5.8h, v2.16b, #0
	ushll	v2.8h, v2.8b, #0
	mov	v4.s[1], wzr
	umull2	v7.4s, v6.8h, v2.8h
	umull	v2.4s, v6.4h, v2.4h
	mov	v4.s[2], wzr
	umlal2	v7.4s, v0.8h, v1.8h
	umlal	v2.4s, v0.4h, v1.4h
	mov	v4.s[3], wzr
	umlal2	v7.4s, v3.8h, v5.8h
	umlal	v4.4s, v3.4h, v5.4h
	add	v0.4s, v2.4s, v4.4s
	add	v0.4s, v0.4s, v7.4s
	addv	s0, v0.4s
	fmov	w8, s0
	add	w0, w8, w2
	ret
.Lfunc_end31:
	.size	test_udot_v25i8, .Lfunc_end31-test_udot_v25i8
	.cfi_endproc
                                        // -- End function
	.globl	test_udot_v25i8_nomla           // -- Begin function test_udot_v25i8_nomla
	.p2align	2
	.type	test_udot_v25i8_nomla,@function
test_udot_v25i8_nomla:                  // @test_udot_v25i8_nomla
	.cfi_startproc
// %bb.0:                               // %entry
	ldp	q1, q0, [x0]
	ushll	v3.8h, v1.8b, #0
	ushll2	v1.8h, v1.16b, #0
	ushll2	v2.8h, v0.16b, #0
	ushll	v0.8h, v0.8b, #0
	ushll	v2.4s, v2.4h, #0
	uaddl	v4.4s, v3.4h, v0.4h
	mov	v2.s[1], wzr
	uaddl2	v0.4s, v3.8h, v0.8h
	mov	v2.s[2], wzr
	uaddw2	v0.4s, v0.4s, v1.8h
	mov	v2.s[3], wzr
	uaddw	v2.4s, v2.4s, v1.4h
	add	v1.4s, v4.4s, v2.4s
	add	v0.4s, v1.4s, v0.4s
	addv	s0, v0.4s
	fmov	w0, s0
	ret
.Lfunc_end32:
	.size	test_udot_v25i8_nomla, .Lfunc_end32-test_udot_v25i8_nomla
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v25i8                 // -- Begin function test_sdot_v25i8
	.p2align	2
	.type	test_sdot_v25i8,@function
test_sdot_v25i8:                        // @test_sdot_v25i8
	.cfi_startproc
// %bb.0:                               // %entry
	ldp	q3, q0, [x1]
	sshll	v6.8h, v3.8b, #0
	sshll2	v3.8h, v3.16b, #0
	ldp	q2, q1, [x0]
	sshll2	v5.8h, v0.16b, #0
	sshll	v0.8h, v0.8b, #0
	sshll2	v4.8h, v1.16b, #0
	sshll	v1.8h, v1.8b, #0
	smull	v4.4s, v5.4h, v4.4h
	sshll2	v5.8h, v2.16b, #0
	sshll	v2.8h, v2.8b, #0
	mov	v4.s[1], wzr
	smull2	v7.4s, v6.8h, v2.8h
	smull	v2.4s, v6.4h, v2.4h
	mov	v4.s[2], wzr
	smlal2	v7.4s, v0.8h, v1.8h
	smlal	v2.4s, v0.4h, v1.4h
	mov	v4.s[3], wzr
	smlal2	v7.4s, v3.8h, v5.8h
	smlal	v4.4s, v3.4h, v5.4h
	add	v0.4s, v2.4s, v4.4s
	add	v0.4s, v0.4s, v7.4s
	addv	s0, v0.4s
	fmov	w8, s0
	add	w0, w8, w2
	ret
.Lfunc_end33:
	.size	test_sdot_v25i8, .Lfunc_end33-test_sdot_v25i8
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v25i8_double          // -- Begin function test_sdot_v25i8_double
	.p2align	2
	.type	test_sdot_v25i8_double,@function
test_sdot_v25i8_double:                 // @test_sdot_v25i8_double
	.cfi_startproc
// %bb.0:                               // %entry
	ldr	b2, [sp, #64]
	add	x8, sp, #72
	ldr	b0, [sp]
	add	x9, sp, #80
	ldr	b4, [sp, #264]
	add	x10, sp, #272
	ld1	{ v2.b }[1], [x8]
	add	x8, sp, #8
	add	x11, sp, #288
	add	x12, sp, #40
	ld1	{ v4.b }[1], [x10]
	add	x10, sp, #280
	ld1	{ v0.b }[1], [x8]
	add	x8, sp, #88
	ld1	{ v2.b }[2], [x9]
	add	x9, sp, #16
	ldr	b1, [sp, #200]
	fmov	s6, w0
	ld1	{ v4.b }[2], [x10]
	add	x10, sp, #112
	ld1	{ v0.b }[2], [x9]
	add	x9, sp, #96
	ld1	{ v2.b }[3], [x8]
	add	x8, sp, #24
	ldr	b16, [sp, #136]
	ld1	{ v4.b }[3], [x11]
	add	x11, sp, #208
	ld1	{ v0.b }[3], [x8]
	add	x8, sp, #104
	ld1	{ v2.b }[4], [x9]
	add	x9, sp, #32
	ld1	{ v1.b }[1], [x11]
	add	x11, sp, #312
	ldr	b3, [sp, #464]
	ld1	{ v0.b }[4], [x9]
	add	x9, sp, #48
	ld1	{ v2.b }[5], [x8]
	add	x8, sp, #120
	mov	v6.b[1], w1
	ldr	b7, [sp, #336]
	ldr	b19, [sp, #536]
	ld1	{ v0.b }[5], [x12]
	add	x12, sp, #56
	ld1	{ v2.b }[6], [x10]
	add	x10, sp, #144
	mov	v6.b[2], w2
	ldr	b5, [sp, #128]
	ldr	b17, [sp, #328]
	ld1	{ v0.b }[6], [x9]
	add	x9, sp, #216
	ld1	{ v16.b }[1], [x10]
	add	x10, sp, #296
	ld1	{ v2.b }[7], [x8]
	add	x8, sp, #152
	ld1	{ v1.b }[2], [x9]
	add	x9, sp, #224
	ld1	{ v4.b }[4], [x10]
	add	x10, sp, #304
	ld1	{ v16.b }[2], [x8]
	add	x8, sp, #160
	mov	v6.b[3], w3
	ldr	b20, [sp, #528]
	ld1	{ v1.b }[3], [x9]
	add	x9, sp, #232
	ld1	{ v4.b }[5], [x10]
	add	x10, sp, #472
	ld1	{ v16.b }[3], [x8]
	add	x8, sp, #168
	mov	v6.b[4], w4
	ld1	{ v0.b }[7], [x12]
	ld1	{ v1.b }[4], [x9]
	add	x9, sp, #240
	ld1	{ v3.b }[1], [x10]
	add	x10, sp, #480
	ld1	{ v16.b }[4], [x8]
	add	x8, sp, #176
	mov	v6.b[5], w5
	ld1	{ v4.b }[6], [x11]
	ld1	{ v1.b }[5], [x9]
	add	x9, sp, #248
	ld1	{ v3.b }[2], [x10]
	add	x10, sp, #488
	ld1	{ v16.b }[5], [x8]
	add	x8, sp, #184
	mov	v6.b[6], w6
	add	x11, sp, #320
	ld1	{ v1.b }[6], [x9]
	add	x9, sp, #256
	ld1	{ v3.b }[3], [x10]
	add	x10, sp, #512
	ld1	{ v16.b }[6], [x8]
	add	x8, sp, #496
	mov	v6.b[7], w7
	ld1	{ v4.b }[7], [x11]
	ld1	{ v1.b }[7], [x9]
	add	x9, sp, #192
	ld1	{ v3.b }[4], [x8]
	add	x8, sp, #504
	sshll	v5.8h, v5.8b, #0
	add	x11, sp, #672
	ld1	{ v16.b }[7], [x9]
	add	x9, sp, #344
	sshll	v6.8h, v6.8b, #0
	ld1	{ v3.b }[5], [x8]
	add	x8, sp, #352
	ld1	{ v7.b }[1], [x9]
	add	x9, sp, #544
	sshll	v16.8h, v16.8b, #0
	sshll	v17.8h, v17.8b, #0
	ld1	{ v19.b }[1], [x9]
	add	x9, sp, #360
	ld1	{ v7.b }[2], [x8]
	add	x8, sp, #552
	smull2	v18.4s, v6.8h, v16.8h
	ld1	{ v3.b }[6], [x10]
	smull	v6.4s, v6.4h, v16.4h
	ldr	b16, [sp, #400]
	ld1	{ v19.b }[2], [x8]
	add	x8, sp, #560
	ld1	{ v7.b }[3], [x9]
	add	x9, sp, #368
	add	x10, sp, #408
	sshll	v2.8h, v2.8b, #0
	ld1	{ v19.b }[3], [x8]
	add	x8, sp, #568
	ld1	{ v7.b }[4], [x9]
	add	x9, sp, #376
	sshll	v4.8h, v4.8b, #0
	ld1	{ v16.b }[1], [x10]
	smull	v5.4s, v5.4h, v17.4h
	ldr	b17, [sp, #664]
	ld1	{ v19.b }[4], [x8]
	add	x8, sp, #576
	ld1	{ v7.b }[5], [x9]
	add	x9, sp, #384
	add	x10, sp, #416
	ld1	{ v17.b }[1], [x11]
	smlal	v6.4s, v2.4h, v4.4h
	add	x11, sp, #680
	ld1	{ v19.b }[5], [x8]
	add	x8, sp, #584
	ld1	{ v7.b }[6], [x9]
	add	x9, sp, #392
	smlal2	v18.4s, v2.8h, v4.8h
	ldr	b2, [sp, #600]
	ld1	{ v16.b }[2], [x10]
	add	x10, sp, #424
	ld1	{ v19.b }[6], [x8]
	add	x8, sp, #592
	ld1	{ v7.b }[7], [x9]
	add	x9, sp, #608
	ld1	{ v17.b }[2], [x11]
	add	x11, sp, #520
	ld1	{ v16.b }[3], [x10]
	add	x10, sp, #688
	ld1	{ v2.b }[1], [x9]
	add	x9, sp, #616
	ld1	{ v19.b }[7], [x8]
	add	x8, sp, #432
	ld1	{ v17.b }[3], [x10]
	add	x10, sp, #696
	sshll	v4.8h, v7.8b, #0
	ld1	{ v3.b }[7], [x11]
	ld1	{ v2.b }[2], [x9]
	add	x9, sp, #624
	sshll	v7.8h, v19.8b, #0
	ld1	{ v16.b }[4], [x8]
	ld1	{ v17.b }[4], [x10]
	add	x10, sp, #704
	smull2	v19.4s, v4.8h, v7.8h
	add	x8, sp, #440
	ld1	{ v2.b }[3], [x9]
	add	x9, sp, #632
	smull	v4.4s, v4.4h, v7.4h
	ldr	b7, [sp, #728]
	sshll	v20.8h, v20.8b, #0
	ld1	{ v17.b }[5], [x10]
	add	x10, sp, #712
	ld1	{ v16.b }[5], [x8]
	sshll	v7.8h, v7.8b, #0
	ld1	{ v2.b }[4], [x9]
	add	x9, sp, #640
	add	x8, sp, #448
	smull	v7.4s, v20.4h, v7.4h
	ld1	{ v17.b }[6], [x10]
	mov	v5.s[1], wzr
	add	x10, sp, #720
	ld1	{ v2.b }[5], [x9]
	add	x9, sp, #648
	ld1	{ v16.b }[6], [x8]
	add	x8, sp, #456
	mov	v7.s[1], wzr
	ld1	{ v17.b }[7], [x10]
	mov	v5.s[2], wzr
	ld1	{ v2.b }[6], [x9]
	add	x9, sp, #656
	ld1	{ v16.b }[7], [x8]
	mov	v7.s[2], wzr
	sshll	v3.8h, v3.8b, #0
	ld1	{ v2.b }[7], [x9]
	sshll	v17.8h, v17.8b, #0
	mov	v5.s[3], wzr
	mov	v7.s[3], wzr
	smlal	v4.4s, v3.4h, v17.4h
	smlal2	v19.4s, v3.8h, v17.8h
	sshll	v0.8h, v0.8b, #0
	sshll	v1.8h, v1.8b, #0
	sshll	v3.8h, v16.8b, #0
	sshll	v2.8h, v2.8b, #0
	smlal	v5.4s, v0.4h, v1.4h
	smlal	v7.4s, v3.4h, v2.4h
	smlal2	v18.4s, v0.8h, v1.8h
	smlal2	v19.4s, v3.8h, v2.8h
	add	v0.4s, v6.4s, v5.4s
	add	v1.4s, v4.4s, v7.4s
	add	v0.4s, v0.4s, v18.4s
	add	v1.4s, v1.4s, v19.4s
	add	v0.4s, v0.4s, v1.4s
	addv	s0, v0.4s
	fmov	w0, s0
	ret
.Lfunc_end34:
	.size	test_sdot_v25i8_double, .Lfunc_end34-test_sdot_v25i8_double
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v25i8_double_nomla    // -- Begin function test_sdot_v25i8_double_nomla
	.p2align	2
	.type	test_sdot_v25i8_double_nomla,@function
test_sdot_v25i8_double_nomla:           // @test_sdot_v25i8_double_nomla
	.cfi_startproc
// %bb.0:                               // %entry
	ldr	b0, [sp, #64]
	add	x8, sp, #72
	ldr	b2, [sp]
	add	x9, sp, #8
	ldr	b3, [sp, #464]
	add	x10, sp, #472
	ld1	{ v0.b }[1], [x8]
	add	x8, sp, #80
	ld1	{ v2.b }[1], [x9]
	add	x9, sp, #16
	add	x11, sp, #104
	ld1	{ v3.b }[1], [x10]
	add	x10, sp, #480
	add	x12, sp, #32
	ld1	{ v0.b }[2], [x8]
	add	x8, sp, #88
	ld1	{ v2.b }[2], [x9]
	add	x9, sp, #24
	ldr	b4, [sp, #128]
	fmov	s1, w0
	ld1	{ v3.b }[2], [x10]
	add	x10, sp, #48
	ld1	{ v0.b }[3], [x8]
	add	x8, sp, #96
	ld1	{ v2.b }[3], [x9]
	add	x9, sp, #120
	sshll	v4.8h, v4.8b, #0
	ldr	b6, [sp, #400]
	mov	v1.b[1], w1
	ldr	b16, [sp, #528]
	ld1	{ v0.b }[4], [x8]
	add	x8, sp, #112
	ld1	{ v2.b }[4], [x12]
	add	x12, sp, #40
	sshll	v5.4s, v4.4h, #0
	ldr	b4, [sp, #336]
	mov	v1.b[2], w2
	ld1	{ v0.b }[5], [x11]
	add	x11, sp, #488
	ld1	{ v2.b }[5], [x12]
	sshll	v16.8h, v16.8b, #0
	ld1	{ v3.b }[3], [x11]
	add	x11, sp, #344
	ld1	{ v0.b }[6], [x8]
	add	x8, sp, #496
	ld1	{ v2.b }[6], [x10]
	add	x10, sp, #352
	ld1	{ v4.b }[1], [x11]
	add	x11, sp, #416
	ld1	{ v3.b }[4], [x8]
	add	x8, sp, #504
	ld1	{ v0.b }[7], [x9]
	add	x9, sp, #408
	mov	v1.b[3], w3
	ld1	{ v4.b }[2], [x10]
	add	x10, sp, #360
	ld1	{ v6.b }[1], [x9]
	add	x9, sp, #56
	ld1	{ v3.b }[5], [x8]
	add	x8, sp, #512
	mov	v1.b[4], w4
	ld1	{ v2.b }[7], [x9]
	add	x9, sp, #424
	ld1	{ v6.b }[2], [x11]
	ld1	{ v4.b }[3], [x10]
	add	x10, sp, #368
	ld1	{ v3.b }[6], [x8]
	add	x8, sp, #376
	mov	v1.b[5], w5
	ld1	{ v6.b }[3], [x9]
	add	x9, sp, #432
	ld1	{ v4.b }[4], [x10]
	add	x10, sp, #440
	sshll	v16.4s, v16.4h, #0
	mov	v1.b[6], w6
	ld1	{ v6.b }[4], [x9]
	add	x9, sp, #520
	ld1	{ v4.b }[5], [x8]
	add	x8, sp, #384
	mov	v5.s[1], wzr
	mov	v16.s[1], wzr
	ld1	{ v3.b }[7], [x9]
	ld1	{ v6.b }[5], [x10]
	add	x10, sp, #448
	ld1	{ v4.b }[6], [x8]
	add	x8, sp, #392
	mov	v1.b[7], w7
	add	x9, sp, #456
	mov	v5.s[2], wzr
	ld1	{ v6.b }[6], [x10]
	mov	v16.s[2], wzr
	ld1	{ v4.b }[7], [x8]
	sshll	v0.8h, v0.8b, #0
	sshll	v1.8h, v1.8b, #0
	ld1	{ v6.b }[7], [x9]
	mov	v5.s[3], wzr
	mov	v16.s[3], wzr
	saddl	v7.4s, v1.4h, v0.4h
	saddl2	v0.4s, v1.8h, v0.8h
	sshll	v1.8h, v3.8b, #0
	sshll	v3.8h, v4.8b, #0
	sshll	v2.8h, v2.8b, #0
	sshll	v4.8h, v6.8b, #0
	saddl	v6.4s, v3.4h, v1.4h
	saddl2	v1.4s, v3.8h, v1.8h
	saddw	v5.4s, v5.4s, v2.4h
	saddw	v3.4s, v16.4s, v4.4h
	saddw2	v0.4s, v0.4s, v2.8h
	saddw2	v1.4s, v1.4s, v4.8h
	add	v5.4s, v7.4s, v5.4s
	add	v2.4s, v6.4s, v3.4s
	add	v0.4s, v5.4s, v0.4s
	add	v1.4s, v2.4s, v1.4s
	add	v0.4s, v0.4s, v1.4s
	addv	s0, v0.4s
	fmov	w0, s0
	ret
.Lfunc_end35:
	.size	test_sdot_v25i8_double_nomla, .Lfunc_end35-test_sdot_v25i8_double_nomla
	.cfi_endproc
                                        // -- End function
	.globl	test_udot_v32i8                 // -- Begin function test_udot_v32i8
	.p2align	2
	.type	test_udot_v32i8,@function
test_udot_v32i8:                        // @test_udot_v32i8
	.cfi_startproc
// %bb.0:                               // %entry
	ldp	q3, q2, [x0]
	movi	v0.2d, #0000000000000000
	ldr	q1, [x1, #16]
	udot	v0.4s, v1.16b, v2.16b
	ldr	q1, [x1]
	udot	v0.4s, v1.16b, v3.16b
	addv	s0, v0.4s
	fmov	w8, s0
	add	w0, w8, w2
	ret
.Lfunc_end36:
	.size	test_udot_v32i8, .Lfunc_end36-test_udot_v32i8
	.cfi_endproc
                                        // -- End function
	.globl	test_udot_v32i8_nomla           // -- Begin function test_udot_v32i8_nomla
	.p2align	2
	.type	test_udot_v32i8_nomla,@function
test_udot_v32i8_nomla:                  // @test_udot_v32i8_nomla
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v0.16b, #1
	ldr	q2, [x0, #16]
	movi	v1.2d, #0000000000000000
	udot	v1.4s, v2.16b, v0.16b
	ldr	q2, [x0]
	udot	v1.4s, v2.16b, v0.16b
	addv	s0, v1.4s
	fmov	w0, s0
	ret
.Lfunc_end37:
	.size	test_udot_v32i8_nomla, .Lfunc_end37-test_udot_v32i8_nomla
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v32i8                 // -- Begin function test_sdot_v32i8
	.p2align	2
	.type	test_sdot_v32i8,@function
test_sdot_v32i8:                        // @test_sdot_v32i8
	.cfi_startproc
// %bb.0:                               // %entry
	ldp	q3, q2, [x0]
	movi	v0.2d, #0000000000000000
	ldr	q1, [x1, #16]
	sdot	v0.4s, v1.16b, v2.16b
	ldr	q1, [x1]
	sdot	v0.4s, v1.16b, v3.16b
	addv	s0, v0.4s
	fmov	w8, s0
	add	w0, w8, w2
	ret
.Lfunc_end38:
	.size	test_sdot_v32i8, .Lfunc_end38-test_sdot_v32i8
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v32i8_double          // -- Begin function test_sdot_v32i8_double
	.p2align	2
	.type	test_sdot_v32i8_double,@function
test_sdot_v32i8_double:                 // @test_sdot_v32i8_double
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v16.2d, #0000000000000000
	movi	v17.2d, #0000000000000000
	sdot	v16.4s, v1.16b, v3.16b
	sdot	v17.4s, v5.16b, v7.16b
	sdot	v16.4s, v0.16b, v2.16b
	sdot	v17.4s, v4.16b, v6.16b
	add	v0.4s, v16.4s, v17.4s
	addv	s0, v0.4s
	fmov	w0, s0
	ret
.Lfunc_end39:
	.size	test_sdot_v32i8_double, .Lfunc_end39-test_sdot_v32i8_double
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v32i8_double_nomla    // -- Begin function test_sdot_v32i8_double_nomla
	.p2align	2
	.type	test_sdot_v32i8_double_nomla,@function
test_sdot_v32i8_double_nomla:           // @test_sdot_v32i8_double_nomla
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v2.16b, #1
	movi	v3.2d, #0000000000000000
	movi	v6.2d, #0000000000000000
	sdot	v3.4s, v1.16b, v2.16b
	sdot	v6.4s, v5.16b, v2.16b
	sdot	v3.4s, v0.16b, v2.16b
	sdot	v6.4s, v4.16b, v2.16b
	add	v0.4s, v3.4s, v6.4s
	addv	s0, v0.4s
	fmov	w0, s0
	ret
.Lfunc_end40:
	.size	test_sdot_v32i8_double_nomla, .Lfunc_end40-test_sdot_v32i8_double_nomla
	.cfi_endproc
                                        // -- End function
	.globl	test_udot_v33i8                 // -- Begin function test_udot_v33i8
	.p2align	2
	.type	test_udot_v33i8,@function
test_udot_v33i8:                        // @test_udot_v33i8
	.cfi_startproc
// %bb.0:                               // %entry
	ldr	b0, [x1, #32]
	ldr	b1, [x0, #32]
	ldp	q2, q3, [x0]
	ushll	v0.8h, v0.8b, #0
	ushll	v1.8h, v1.8b, #0
	umull	v0.4s, v0.4h, v1.4h
	ushll	v6.8h, v2.8b, #0
	ldp	q1, q4, [x1]
	ushll2	v2.8h, v2.16b, #0
	mov	v0.s[1], wzr
	ushll2	v5.8h, v3.16b, #0
	ushll2	v16.8h, v1.16b, #0
	ushll	v1.8h, v1.8b, #0
	mov	v0.s[2], wzr
	umull2	v17.4s, v16.8h, v2.8h
	umull2	v18.4s, v1.8h, v6.8h
	ushll	v3.8h, v3.8b, #0
	mov	v0.s[3], wzr
	ushll2	v7.8h, v4.16b, #0
	ushll	v4.8h, v4.8b, #0
	umlal2	v17.4s, v7.8h, v5.8h
	umlal	v0.4s, v1.4h, v6.4h
	umull	v1.4s, v16.4h, v2.4h
	umlal2	v18.4s, v4.8h, v3.8h
	umlal	v0.4s, v4.4h, v3.4h
	umlal	v1.4s, v7.4h, v5.4h
	add	v2.4s, v18.4s, v17.4s
	add	v0.4s, v0.4s, v1.4s
	add	v0.4s, v0.4s, v2.4s
	addv	s0, v0.4s
	fmov	w8, s0
	add	w0, w8, w2
	ret
.Lfunc_end41:
	.size	test_udot_v33i8, .Lfunc_end41-test_udot_v33i8
	.cfi_endproc
                                        // -- End function
	.globl	test_udot_v33i8_nomla           // -- Begin function test_udot_v33i8_nomla
	.p2align	2
	.type	test_udot_v33i8_nomla,@function
test_udot_v33i8_nomla:                  // @test_udot_v33i8_nomla
	.cfi_startproc
// %bb.0:                               // %entry
	ldr	b0, [x0, #32]
	ldp	q2, q1, [x0]
	ushll	v0.8h, v0.8b, #0
	ushll	v0.4s, v0.4h, #0
	ushll2	v4.8h, v2.16b, #0
	mov	v0.s[1], wzr
	ushll	v2.8h, v2.8b, #0
	ushll	v3.8h, v1.8b, #0
	ushll2	v1.8h, v1.16b, #0
	mov	v0.s[2], wzr
	uaddl2	v5.4s, v4.8h, v1.8h
	uaddl	v1.4s, v4.4h, v1.4h
	mov	v0.s[3], wzr
	uaddw	v0.4s, v0.4s, v2.4h
	uaddl2	v2.4s, v2.8h, v3.8h
	uaddw	v0.4s, v0.4s, v3.4h
	add	v2.4s, v2.4s, v5.4s
	add	v0.4s, v0.4s, v1.4s
	add	v0.4s, v0.4s, v2.4s
	addv	s0, v0.4s
	fmov	w0, s0
	ret
.Lfunc_end42:
	.size	test_udot_v33i8_nomla, .Lfunc_end42-test_udot_v33i8_nomla
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v33i8                 // -- Begin function test_sdot_v33i8
	.p2align	2
	.type	test_sdot_v33i8,@function
test_sdot_v33i8:                        // @test_sdot_v33i8
	.cfi_startproc
// %bb.0:                               // %entry
	ldr	b0, [x1, #32]
	ldr	b1, [x0, #32]
	ldp	q2, q3, [x0]
	sshll	v0.8h, v0.8b, #0
	sshll	v1.8h, v1.8b, #0
	smull	v0.4s, v0.4h, v1.4h
	sshll	v6.8h, v2.8b, #0
	ldp	q1, q4, [x1]
	sshll2	v2.8h, v2.16b, #0
	mov	v0.s[1], wzr
	sshll2	v5.8h, v3.16b, #0
	sshll2	v16.8h, v1.16b, #0
	sshll	v1.8h, v1.8b, #0
	mov	v0.s[2], wzr
	smull2	v17.4s, v16.8h, v2.8h
	smull2	v18.4s, v1.8h, v6.8h
	sshll	v3.8h, v3.8b, #0
	mov	v0.s[3], wzr
	sshll2	v7.8h, v4.16b, #0
	sshll	v4.8h, v4.8b, #0
	smlal2	v17.4s, v7.8h, v5.8h
	smlal	v0.4s, v1.4h, v6.4h
	smull	v1.4s, v16.4h, v2.4h
	smlal2	v18.4s, v4.8h, v3.8h
	smlal	v0.4s, v4.4h, v3.4h
	smlal	v1.4s, v7.4h, v5.4h
	add	v2.4s, v18.4s, v17.4s
	add	v0.4s, v0.4s, v1.4s
	add	v0.4s, v0.4s, v2.4s
	addv	s0, v0.4s
	fmov	w8, s0
	add	w0, w8, w2
	ret
.Lfunc_end43:
	.size	test_sdot_v33i8, .Lfunc_end43-test_sdot_v33i8
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v33i8_double          // -- Begin function test_sdot_v33i8_double
	.p2align	2
	.type	test_sdot_v33i8_double,@function
test_sdot_v33i8_double:                 // @test_sdot_v33i8_double
	.cfi_startproc
// %bb.0:                               // %entry
	ldr	b0, [sp, #64]
	add	x8, sp, #72
	ldr	b3, [sp]
	add	x9, sp, #136
	ldr	b2, [sp, #128]
	add	x10, sp, #88
	ld1	{ v0.b }[1], [x8]
	add	x8, sp, #80
	ldr	b4, [sp, #328]
	add	x11, sp, #352
	ld1	{ v2.b }[1], [x9]
	add	x9, sp, #144
	ldr	b7, [sp, #200]
	fmov	s1, w0
	ld1	{ v0.b }[2], [x8]
	add	x8, sp, #8
	ldr	b17, [sp, #264]
	ld1	{ v2.b }[2], [x9]
	add	x9, sp, #152
	ld1	{ v3.b }[1], [x8]
	add	x8, sp, #16
	ld1	{ v0.b }[3], [x10]
	add	x10, sp, #96
	ldr	b16, [sp, #392]
	ld1	{ v2.b }[3], [x9]
	add	x9, sp, #160
	ld1	{ v3.b }[2], [x8]
	add	x8, sp, #24
	ld1	{ v0.b }[4], [x10]
	add	x10, sp, #104
	ldr	b5, [sp, #192]
	ld1	{ v2.b }[4], [x9]
	add	x9, sp, #168
	ld1	{ v3.b }[3], [x8]
	add	x8, sp, #32
	ld1	{ v0.b }[5], [x10]
	add	x10, sp, #112
	sshll	v5.8h, v5.8b, #0
	ldr	b20, [sp, #856]
	ld1	{ v2.b }[5], [x9]
	add	x9, sp, #176
	ld1	{ v3.b }[4], [x8]
	add	x8, sp, #40
	ld1	{ v0.b }[6], [x10]
	add	x10, sp, #48
	mov	v1.b[1], w1
	ldr	b21, [sp, #728]
	ld1	{ v2.b }[6], [x9]
	add	x9, sp, #184
	ld1	{ v3.b }[5], [x8]
	add	x8, sp, #120
	mov	v1.b[2], w2
	ld1	{ v0.b }[7], [x8]
	add	x8, sp, #56
	ld1	{ v3.b }[6], [x10]
	add	x10, sp, #272
	ld1	{ v2.b }[7], [x9]
	add	x9, sp, #208
	mov	v1.b[3], w3
	ld1	{ v17.b }[1], [x10]
	add	x10, sp, #224
	ld1	{ v3.b }[7], [x8]
	add	x8, sp, #336
	ld1	{ v7.b }[1], [x9]
	add	x9, sp, #400
	sshll	v19.8h, v2.8b, #0
	ldr	b2, [sp, #456]
	ld1	{ v4.b }[1], [x8]
	add	x8, sp, #344
	ld1	{ v16.b }[1], [x9]
	add	x9, sp, #408
	sshll	v3.8h, v3.8b, #0
	mov	v1.b[4], w4
	ld1	{ v4.b }[2], [x8]
	add	x8, sp, #216
	ld1	{ v16.b }[2], [x9]
	add	x9, sp, #416
	sshll	v0.8h, v0.8b, #0
	ld1	{ v7.b }[2], [x8]
	add	x8, sp, #280
	ld1	{ v4.b }[3], [x11]
	add	x11, sp, #360
	ld1	{ v16.b }[3], [x9]
	add	x9, sp, #424
	ld1	{ v17.b }[2], [x8]
	add	x8, sp, #288
	ld1	{ v7.b }[3], [x10]
	add	x10, sp, #232
	ld1	{ v4.b }[4], [x11]
	add	x11, sp, #368
	ld1	{ v16.b }[4], [x9]
	add	x9, sp, #432
	ld1	{ v17.b }[3], [x8]
	add	x8, sp, #296
	ld1	{ v7.b }[4], [x10]
	add	x10, sp, #240
	ld1	{ v4.b }[5], [x11]
	add	x11, sp, #376
	ld1	{ v16.b }[5], [x9]
	add	x9, sp, #440
	ld1	{ v17.b }[4], [x8]
	add	x8, sp, #304
	ld1	{ v7.b }[5], [x10]
	add	x10, sp, #248
	ld1	{ v4.b }[6], [x11]
	add	x11, sp, #384
	ld1	{ v16.b }[6], [x9]
	add	x9, sp, #448
	ld1	{ v17.b }[5], [x8]
	add	x8, sp, #312
	ld1	{ v7.b }[6], [x10]
	add	x10, sp, #256
	ld1	{ v4.b }[7], [x11]
	add	x11, sp, #480
	ld1	{ v16.b }[7], [x9]
	add	x9, sp, #472
	ld1	{ v17.b }[6], [x8]
	add	x8, sp, #320
	ld1	{ v7.b }[7], [x10]
	add	x10, sp, #624
	sshll	v6.8h, v4.8b, #0
	sshll	v16.8h, v16.8b, #0
	ld1	{ v17.b }[7], [x8]
	add	x8, sp, #600
	sshll	v18.8h, v7.8b, #0
	ldr	b7, [sp, #592]
	mov	v1.b[5], w5
	ld1	{ v7.b }[1], [x8]
	add	x8, sp, #608
	sshll	v4.8h, v17.8b, #0
	sshll	v17.8h, v2.8b, #0
	smull2	v2.4s, v3.8h, v4.8h
	smull	v3.4s, v3.4h, v4.4h
	ld1	{ v7.b }[2], [x8]
	add	x8, sp, #616
	smull	v4.4s, v5.4h, v17.4h
	ldr	b17, [sp, #528]
	smlal2	v2.4s, v19.8h, v16.8h
	smlal	v3.4s, v19.4h, v16.4h
	ldr	b16, [sp, #464]
	ld1	{ v7.b }[3], [x8]
	add	x8, sp, #536
	ldr	b19, [sp, #656]
	ld1	{ v16.b }[1], [x9]
	add	x9, sp, #664
	ld1	{ v17.b }[1], [x8]
	add	x8, sp, #544
	ld1	{ v7.b }[4], [x10]
	add	x10, sp, #632
	ld1	{ v19.b }[1], [x9]
	add	x9, sp, #672
	ld1	{ v16.b }[2], [x11]
	add	x11, sp, #488
	ld1	{ v17.b }[2], [x8]
	add	x8, sp, #552
	ld1	{ v7.b }[5], [x10]
	add	x10, sp, #640
	ld1	{ v19.b }[2], [x9]
	add	x9, sp, #680
	ld1	{ v16.b }[3], [x11]
	add	x11, sp, #496
	ld1	{ v17.b }[3], [x8]
	add	x8, sp, #560
	mov	v4.s[1], wzr
	ld1	{ v7.b }[6], [x10]
	ld1	{ v19.b }[3], [x9]
	add	x9, sp, #688
	ld1	{ v16.b }[4], [x11]
	add	x11, sp, #504
	ld1	{ v17.b }[4], [x8]
	add	x8, sp, #568
	mov	v1.b[6], w6
	add	x10, sp, #648
	ld1	{ v19.b }[4], [x9]
	add	x9, sp, #696
	mov	v4.s[2], wzr
	ld1	{ v16.b }[5], [x11]
	ld1	{ v17.b }[5], [x8]
	add	x8, sp, #864
	mov	v1.b[7], w7
	ld1	{ v7.b }[7], [x10]
	ld1	{ v19.b }[5], [x9]
	add	x9, sp, #704
	ld1	{ v20.b }[1], [x8]
	add	x10, sp, #512
	add	x8, sp, #872
	add	x11, sp, #576
	mov	v4.s[3], wzr
	ld1	{ v19.b }[6], [x9]
	add	x9, sp, #712
	ld1	{ v16.b }[6], [x10]
	add	x10, sp, #520
	ld1	{ v20.b }[2], [x8]
	add	x8, sp, #736
	sshll	v1.8h, v1.8b, #0
	ld1	{ v17.b }[6], [x11]
	ld1	{ v19.b }[7], [x9]
	add	x9, sp, #928
	smull2	v5.4s, v1.8h, v18.8h
	ld1	{ v21.b }[1], [x8]
	smlal	v4.4s, v1.4h, v18.4h
	ldr	b1, [sp, #920]
	add	x11, sp, #584
	ld1	{ v16.b }[7], [x10]
	add	x10, sp, #880
	add	x8, sp, #744
	ld1	{ v1.b }[1], [x9]
	add	x9, sp, #936
	ld1	{ v17.b }[7], [x11]
	add	x11, sp, #800
	smlal2	v5.4s, v0.8h, v6.8h
	ld1	{ v20.b }[3], [x10]
	smlal	v4.4s, v0.4h, v6.4h
	ld1	{ v21.b }[2], [x8]
	ldr	b0, [sp, #792]
	add	x10, sp, #888
	add	x8, sp, #752
	ld1	{ v1.b }[2], [x9]
	add	x9, sp, #944
	ldr	b18, [sp, #720]
	ld1	{ v0.b }[1], [x11]
	add	x11, sp, #808
	ld1	{ v20.b }[4], [x10]
	add	x10, sp, #896
	ld1	{ v21.b }[3], [x8]
	add	x8, sp, #760
	ld1	{ v1.b }[3], [x9]
	add	x9, sp, #952
	ld1	{ v0.b }[2], [x11]
	add	x11, sp, #816
	ld1	{ v20.b }[5], [x10]
	add	x10, sp, #904
	ld1	{ v21.b }[4], [x8]
	add	x8, sp, #768
	sshll	v6.8h, v7.8b, #0
	ld1	{ v1.b }[4], [x9]
	sshll	v7.8h, v16.8b, #0
	ld1	{ v0.b }[3], [x11]
	sshll	v16.8h, v18.8b, #0
	ldr	b18, [sp, #984]
	ld1	{ v20.b }[6], [x10]
	add	x9, sp, #960
	ld1	{ v21.b }[5], [x8]
	add	x8, sp, #824
	sshll	v18.8h, v18.8b, #0
	add	x10, sp, #912
	ld1	{ v1.b }[5], [x9]
	add	x9, sp, #776
	smull	v16.4s, v16.4h, v18.4h
	ld1	{ v0.b }[4], [x8]
	ld1	{ v20.b }[7], [x10]
	add	x10, sp, #968
	add	x8, sp, #832
	ld1	{ v21.b }[6], [x9]
	add	x9, sp, #784
	mov	v16.s[1], wzr
	ld1	{ v1.b }[6], [x10]
	ld1	{ v0.b }[5], [x8]
	add	x10, sp, #976
	add	x8, sp, #840
	ld1	{ v21.b }[7], [x9]
	sshll	v18.8h, v19.8b, #0
	mov	v16.s[2], wzr
	ld1	{ v1.b }[7], [x10]
	ld1	{ v0.b }[6], [x8]
	add	x8, sp, #848
	sshll	v19.8h, v20.8b, #0
	sshll	v21.8h, v21.8b, #0
	mov	v16.s[3], wzr
	sshll	v1.8h, v1.8b, #0
	ld1	{ v0.b }[7], [x8]
	sshll	v17.8h, v17.8b, #0
	smull	v20.4s, v18.4h, v1.4h
	smull2	v1.4s, v18.8h, v1.8h
	smull2	v18.4s, v6.8h, v19.8h
	smlal	v16.4s, v7.4h, v21.4h
	sshll	v0.8h, v0.8b, #0
	smlal2	v18.4s, v7.8h, v21.8h
	smlal2	v1.4s, v17.8h, v0.8h
	smlal	v20.4s, v17.4h, v0.4h
	smlal	v16.4s, v6.4h, v19.4h
	add	v0.4s, v5.4s, v2.4s
	add	v2.4s, v4.4s, v3.4s
	add	v1.4s, v18.4s, v1.4s
	add	v3.4s, v16.4s, v20.4s
	add	v0.4s, v2.4s, v0.4s
	add	v1.4s, v3.4s, v1.4s
	add	v0.4s, v0.4s, v1.4s
	addv	s0, v0.4s
	fmov	w0, s0
	ret
.Lfunc_end44:
	.size	test_sdot_v33i8_double, .Lfunc_end44-test_sdot_v33i8_double
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v33i8_double_nomla    // -- Begin function test_sdot_v33i8_double_nomla
	.p2align	2
	.type	test_sdot_v33i8_double_nomla,@function
test_sdot_v33i8_double_nomla:           // @test_sdot_v33i8_double_nomla
	.cfi_startproc
// %bb.0:                               // %entry
	ldr	b0, [sp, #64]
	add	x8, sp, #72
	ldr	b2, [sp, #128]
	add	x9, sp, #136
	fmov	s3, w0
	ldr	b4, [sp]
	ld1	{ v0.b }[1], [x8]
	add	x8, sp, #80
	ld1	{ v2.b }[1], [x9]
	add	x10, sp, #8
	add	x9, sp, #144
	ldr	b1, [sp, #592]
	mov	v3.b[1], w1
	add	x11, sp, #184
	ld1	{ v0.b }[2], [x8]
	add	x8, sp, #88
	ld1	{ v4.b }[1], [x10]
	add	x10, sp, #16
	ld1	{ v2.b }[2], [x9]
	add	x9, sp, #152
	mov	v3.b[2], w2
	ldr	b5, [sp, #192]
	ld1	{ v0.b }[3], [x8]
	add	x8, sp, #96
	ld1	{ v4.b }[2], [x10]
	add	x10, sp, #24
	ld1	{ v2.b }[3], [x9]
	add	x9, sp, #160
	mov	v3.b[3], w3
	ldr	b17, [sp, #656]
	ld1	{ v0.b }[4], [x8]
	add	x8, sp, #104
	ld1	{ v4.b }[3], [x10]
	add	x10, sp, #32
	ld1	{ v2.b }[4], [x9]
	add	x9, sp, #168
	mov	v3.b[4], w4
	ldr	b18, [sp, #720]
	ld1	{ v0.b }[5], [x8]
	add	x8, sp, #112
	ld1	{ v4.b }[4], [x10]
	add	x10, sp, #120
	ld1	{ v2.b }[5], [x9]
	add	x9, sp, #40
	mov	v3.b[5], w5
	ld1	{ v0.b }[6], [x8]
	add	x8, sp, #176
	ld1	{ v4.b }[5], [x9]
	add	x9, sp, #48
	ld1	{ v2.b }[6], [x8]
	add	x8, sp, #600
	mov	v3.b[6], w6
	ld1	{ v0.b }[7], [x10]
	ld1	{ v4.b }[6], [x9]
	add	x9, sp, #56
	ld1	{ v1.b }[1], [x8]
	add	x8, sp, #608
	ld1	{ v2.b }[7], [x11]
	add	x11, sp, #480
	mov	v3.b[7], w7
	add	x10, sp, #632
	ld1	{ v4.b }[7], [x9]
	add	x9, sp, #472
	ld1	{ v1.b }[2], [x8]
	add	x8, sp, #616
	sshll	v0.8h, v0.8b, #0
	sshll	v6.8h, v2.8b, #0
	sshll	v2.8h, v3.8b, #0
	sshll	v7.8h, v4.8b, #0
	ld1	{ v1.b }[3], [x8]
	sshll	v3.8h, v5.8b, #0
	add	x8, sp, #624
	saddl2	v4.4s, v7.8h, v6.8h
	ldr	b5, [sp, #464]
	saddl2	v16.4s, v2.8h, v0.8h
	ld1	{ v1.b }[4], [x8]
	add	x8, sp, #536
	ld1	{ v5.b }[1], [x9]
	add	x9, sp, #664
	add	v4.4s, v16.4s, v4.4s
	ldr	b16, [sp, #528]
	sshll	v3.4s, v3.4h, #0
	ld1	{ v17.b }[1], [x9]
	add	x9, sp, #672
	ld1	{ v16.b }[1], [x8]
	add	x8, sp, #544
	ld1	{ v5.b }[2], [x11]
	add	x11, sp, #488
	ld1	{ v1.b }[5], [x10]
	add	x10, sp, #640
	ld1	{ v17.b }[2], [x9]
	add	x9, sp, #680
	ld1	{ v16.b }[2], [x8]
	add	x8, sp, #552
	ld1	{ v5.b }[3], [x11]
	add	x11, sp, #496
	ld1	{ v1.b }[6], [x10]
	add	x10, sp, #648
	ld1	{ v17.b }[3], [x9]
	add	x9, sp, #688
	ld1	{ v16.b }[3], [x8]
	add	x8, sp, #560
	ld1	{ v5.b }[4], [x11]
	ld1	{ v1.b }[7], [x10]
	add	x10, sp, #504
	saddl	v6.4s, v7.4h, v6.4h
	ld1	{ v17.b }[4], [x9]
	sshll	v7.8h, v18.8b, #0
	ld1	{ v16.b }[4], [x8]
	mov	v3.s[1], wzr
	add	x8, sp, #568
	ld1	{ v5.b }[5], [x10]
	add	x9, sp, #696
	sshll	v7.4s, v7.4h, #0
	add	x10, sp, #512
	ld1	{ v16.b }[5], [x8]
	add	x8, sp, #576
	mov	v7.s[1], wzr
	ld1	{ v17.b }[5], [x9]
	mov	v3.s[2], wzr
	add	x9, sp, #704
	ld1	{ v5.b }[6], [x10]
	add	x10, sp, #520
	ld1	{ v16.b }[6], [x8]
	add	x8, sp, #584
	mov	v7.s[2], wzr
	ld1	{ v17.b }[6], [x9]
	mov	v3.s[3], wzr
	add	x9, sp, #712
	ld1	{ v5.b }[7], [x10]
	ld1	{ v16.b }[7], [x8]
	mov	v7.s[3], wzr
	ld1	{ v17.b }[7], [x9]
	saddw	v2.4s, v3.4s, v2.4h
	sshll	v5.8h, v5.8b, #0
	sshll	v1.8h, v1.8b, #0
	saddw	v7.4s, v7.4s, v5.4h
	saddw	v0.4s, v2.4s, v0.4h
	sshll	v2.8h, v16.8b, #0
	sshll	v3.8h, v17.8b, #0
	add	v0.4s, v0.4s, v6.4s
	saddl2	v16.4s, v3.8h, v2.8h
	saddl	v2.4s, v3.4h, v2.4h
	saddl2	v3.4s, v1.8h, v5.8h
	saddw	v1.4s, v7.4s, v1.4h
	add	v0.4s, v0.4s, v4.4s
	add	v3.4s, v3.4s, v16.4s
	add	v1.4s, v1.4s, v2.4s
	add	v1.4s, v1.4s, v3.4s
	add	v0.4s, v0.4s, v1.4s
	addv	s0, v0.4s
	fmov	w0, s0
	ret
.Lfunc_end45:
	.size	test_sdot_v33i8_double_nomla, .Lfunc_end45-test_sdot_v33i8_double_nomla
	.cfi_endproc
                                        // -- End function
	.globl	test_udot_v48i8                 // -- Begin function test_udot_v48i8
	.p2align	2
	.type	test_udot_v48i8,@function
test_udot_v48i8:                        // @test_udot_v48i8
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v0.2d, #0000000000000000
	ldr	q1, [x1, #32]
	ldr	q2, [x0, #32]
	udot	v0.4s, v1.16b, v2.16b
	ldr	q1, [x1]
	ldp	q3, q2, [x0]
	udot	v0.4s, v1.16b, v3.16b
	ldr	q1, [x1, #16]
	udot	v0.4s, v1.16b, v2.16b
	addv	s0, v0.4s
	fmov	w8, s0
	add	w0, w8, w2
	ret
.Lfunc_end46:
	.size	test_udot_v48i8, .Lfunc_end46-test_udot_v48i8
	.cfi_endproc
                                        // -- End function
	.globl	test_udot_v48i8_nomla           // -- Begin function test_udot_v48i8_nomla
	.p2align	2
	.type	test_udot_v48i8_nomla,@function
test_udot_v48i8_nomla:                  // @test_udot_v48i8_nomla
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v0.16b, #1
	ldr	q2, [x0, #32]
	movi	v1.2d, #0000000000000000
	udot	v1.4s, v2.16b, v0.16b
	ldr	q2, [x0]
	udot	v1.4s, v2.16b, v0.16b
	ldr	q2, [x0, #16]
	udot	v1.4s, v2.16b, v0.16b
	addv	s0, v1.4s
	fmov	w0, s0
	ret
.Lfunc_end47:
	.size	test_udot_v48i8_nomla, .Lfunc_end47-test_udot_v48i8_nomla
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v48i8                 // -- Begin function test_sdot_v48i8
	.p2align	2
	.type	test_sdot_v48i8,@function
test_sdot_v48i8:                        // @test_sdot_v48i8
	.cfi_startproc
// %bb.0:                               // %entry
	movi	v0.2d, #0000000000000000
	ldr	q1, [x1, #32]
	ldr	q2, [x0, #32]
	sdot	v0.4s, v1.16b, v2.16b
	ldr	q1, [x1]
	ldp	q3, q2, [x0]
	sdot	v0.4s, v1.16b, v3.16b
	ldr	q1, [x1, #16]
	sdot	v0.4s, v1.16b, v2.16b
	addv	s0, v0.4s
	fmov	w8, s0
	add	w0, w8, w2
	ret
.Lfunc_end48:
	.size	test_sdot_v48i8, .Lfunc_end48-test_sdot_v48i8
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v48i8_double          // -- Begin function test_sdot_v48i8_double
	.p2align	2
	.type	test_sdot_v48i8_double,@function
test_sdot_v48i8_double:                 // @test_sdot_v48i8_double
	.cfi_startproc
// %bb.0:                               // %entry
	ldr	b2, [sp, #576]
	add	x8, sp, #584
	ldr	b3, [sp, #192]
	add	x10, sp, #328
	ldr	b0, [sp, #320]
	add	x9, sp, #592
	ld1	{ v2.b }[1], [x8]
	add	x8, sp, #200
	fmov	s1, w0
	add	x11, sp, #648
	ld1	{ v0.b }[1], [x10]
	add	x10, sp, #336
	ld1	{ v3.b }[1], [x8]
	add	x8, sp, #208
	ld1	{ v2.b }[2], [x9]
	add	x9, sp, #600
	mov	v1.b[1], w1
	ldr	b18, [sp, #1344]
	ld1	{ v0.b }[2], [x10]
	add	x10, sp, #224
	ld1	{ v3.b }[2], [x8]
	add	x8, sp, #216
	ld1	{ v2.b }[3], [x9]
	add	x9, sp, #608
	mov	v1.b[2], w2
	ldr	b19, [sp, #960]
	ldr	b4, [sp, #448]
	ld1	{ v3.b }[3], [x8]
	add	x8, sp, #344
	ld1	{ v2.b }[4], [x9]
	add	x9, sp, #616
	mov	v1.b[3], w3
	ldr	b5, [sp, #64]
	ld1	{ v0.b }[3], [x8]
	add	x8, sp, #352
	ld1	{ v3.b }[4], [x10]
	add	x10, sp, #232
	ld1	{ v2.b }[5], [x9]
	add	x9, sp, #360
	mov	v1.b[4], w4
	ldr	b16, [sp, #1088]
	ld1	{ v0.b }[4], [x8]
	add	x8, sp, #624
	ld1	{ v3.b }[5], [x10]
	add	x10, sp, #240
	movi	v7.2d, #0000000000000000
	ldr	b17, [sp, #704]
	ld1	{ v2.b }[6], [x8]
	add	x8, sp, #368
	ld1	{ v0.b }[5], [x9]
	add	x9, sp, #632
	mov	v1.b[5], w5
	ld1	{ v3.b }[6], [x10]
	add	x10, sp, #376
	ld1	{ v2.b }[7], [x9]
	add	x9, sp, #640
	ld1	{ v0.b }[6], [x8]
	add	x8, sp, #248
	mov	v1.b[6], w6
	movi	v6.2d, #0000000000000000
	ld1	{ v3.b }[7], [x8]
	add	x8, sp, #256
	ld1	{ v0.b }[7], [x10]
	mov	x10, sp
	mov	v1.b[7], w7
	ld1	{ v2.b }[8], [x9]
	add	x9, sp, #384
	ld1	{ v3.b }[8], [x8]
	add	x8, sp, #264
	ld1	{ v1.b }[8], [x10]
	add	x10, sp, #8
	ld1	{ v0.b }[8], [x9]
	add	x9, sp, #392
	ld1	{ v3.b }[9], [x8]
	add	x8, sp, #272
	ld1	{ v2.b }[9], [x11]
	add	x11, sp, #656
	ld1	{ v1.b }[9], [x10]
	add	x10, sp, #16
	ld1	{ v0.b }[9], [x9]
	add	x9, sp, #400
	ld1	{ v3.b }[10], [x8]
	add	x8, sp, #280
	ld1	{ v2.b }[10], [x11]
	add	x11, sp, #664
	ld1	{ v1.b }[10], [x10]
	add	x10, sp, #24
	ld1	{ v0.b }[10], [x9]
	add	x9, sp, #408
	ld1	{ v3.b }[11], [x8]
	add	x8, sp, #288
	ld1	{ v2.b }[11], [x11]
	add	x11, sp, #672
	ld1	{ v1.b }[11], [x10]
	add	x10, sp, #32
	ld1	{ v0.b }[11], [x9]
	add	x9, sp, #416
	ld1	{ v3.b }[12], [x8]
	add	x8, sp, #296
	ld1	{ v2.b }[12], [x11]
	add	x11, sp, #680
	ld1	{ v1.b }[12], [x10]
	add	x10, sp, #40
	ld1	{ v0.b }[12], [x9]
	add	x9, sp, #424
	ld1	{ v3.b }[13], [x8]
	add	x8, sp, #304
	ld1	{ v2.b }[13], [x11]
	add	x11, sp, #688
	ld1	{ v1.b }[13], [x10]
	add	x10, sp, #48
	ld1	{ v0.b }[13], [x9]
	add	x9, sp, #432
	ld1	{ v3.b }[14], [x8]
	add	x8, sp, #312
	ld1	{ v2.b }[14], [x11]
	add	x11, sp, #696
	ld1	{ v1.b }[14], [x10]
	add	x10, sp, #456
	ld1	{ v0.b }[14], [x9]
	add	x9, sp, #440
	ld1	{ v3.b }[15], [x8]
	add	x8, sp, #56
	ld1	{ v4.b }[1], [x10]
	add	x10, sp, #72
	ld1	{ v2.b }[15], [x11]
	add	x11, sp, #464
	ld1	{ v1.b }[15], [x8]
	add	x8, sp, #1352
	ld1	{ v0.b }[15], [x9]
	add	x9, sp, #968
	ld1	{ v5.b }[1], [x10]
	add	x10, sp, #80
	ld1	{ v18.b }[1], [x8]
	add	x8, sp, #1360
	ld1	{ v19.b }[1], [x9]
	add	x9, sp, #976
	ld1	{ v4.b }[2], [x11]
	add	x11, sp, #472
	ld1	{ v5.b }[2], [x10]
	add	x10, sp, #88
	ld1	{ v18.b }[2], [x8]
	add	x8, sp, #1368
	ld1	{ v19.b }[2], [x9]
	add	x9, sp, #984
	ld1	{ v4.b }[3], [x11]
	add	x11, sp, #480
	ld1	{ v5.b }[3], [x10]
	add	x10, sp, #96
	ld1	{ v18.b }[3], [x8]
	add	x8, sp, #1376
	ld1	{ v19.b }[3], [x9]
	add	x9, sp, #992
	ld1	{ v4.b }[4], [x11]
	add	x11, sp, #488
	ld1	{ v5.b }[4], [x10]
	add	x10, sp, #104
	ld1	{ v18.b }[4], [x8]
	add	x8, sp, #1384
	ld1	{ v19.b }[4], [x9]
	add	x9, sp, #1000
	ld1	{ v4.b }[5], [x11]
	add	x11, sp, #496
	ld1	{ v5.b }[5], [x10]
	add	x10, sp, #112
	ld1	{ v18.b }[5], [x8]
	add	x8, sp, #1392
	ld1	{ v19.b }[5], [x9]
	add	x9, sp, #1008
	ld1	{ v4.b }[6], [x11]
	add	x11, sp, #504
	ld1	{ v5.b }[6], [x10]
	add	x10, sp, #120
	ld1	{ v18.b }[6], [x8]
	add	x8, sp, #1400
	ld1	{ v19.b }[6], [x9]
	add	x9, sp, #1016
	ld1	{ v4.b }[7], [x11]
	add	x11, sp, #512
	ld1	{ v5.b }[7], [x10]
	add	x10, sp, #128
	ld1	{ v18.b }[7], [x8]
	add	x8, sp, #1408
	ld1	{ v19.b }[7], [x9]
	add	x9, sp, #1024
	ld1	{ v4.b }[8], [x11]
	add	x11, sp, #520
	ld1	{ v5.b }[8], [x10]
	add	x10, sp, #136
	ld1	{ v18.b }[8], [x8]
	add	x8, sp, #1416
	ld1	{ v19.b }[8], [x9]
	add	x9, sp, #1032
	ld1	{ v4.b }[9], [x11]
	add	x11, sp, #528
	ld1	{ v5.b }[9], [x10]
	add	x10, sp, #144
	ld1	{ v18.b }[9], [x8]
	add	x8, sp, #1424
	ld1	{ v19.b }[9], [x9]
	add	x9, sp, #1040
	ld1	{ v4.b }[10], [x11]
	add	x11, sp, #536
	ld1	{ v5.b }[10], [x10]
	add	x10, sp, #152
	ld1	{ v18.b }[10], [x8]
	add	x8, sp, #1432
	ld1	{ v19.b }[10], [x9]
	add	x9, sp, #1048
	ld1	{ v4.b }[11], [x11]
	add	x11, sp, #544
	ld1	{ v5.b }[11], [x10]
	add	x10, sp, #160
	ld1	{ v18.b }[11], [x8]
	add	x8, sp, #1440
	ld1	{ v19.b }[11], [x9]
	add	x9, sp, #1056
	ld1	{ v4.b }[12], [x11]
	add	x11, sp, #552
	ld1	{ v5.b }[12], [x10]
	add	x10, sp, #168
	ld1	{ v18.b }[12], [x8]
	add	x8, sp, #1448
	ld1	{ v19.b }[12], [x9]
	add	x9, sp, #1064
	ld1	{ v4.b }[13], [x11]
	add	x11, sp, #1112
	ld1	{ v5.b }[13], [x10]
	add	x10, sp, #1096
	ld1	{ v18.b }[13], [x8]
	add	x8, sp, #1456
	ld1	{ v19.b }[13], [x9]
	add	x9, sp, #1072
	ld1	{ v16.b }[1], [x10]
	add	x10, sp, #712
	sdot	v7.4s, v3.16b, v2.16b
	ldr	b2, [sp, #1216]
	ld1	{ v18.b }[14], [x8]
	add	x8, sp, #1464
	ld1	{ v19.b }[14], [x9]
	add	x9, sp, #1080
	ld1	{ v17.b }[1], [x10]
	add	x10, sp, #840
	ldr	b3, [sp, #832]
	ld1	{ v18.b }[15], [x8]
	add	x8, sp, #560
	ld1	{ v19.b }[15], [x9]
	add	x9, sp, #176
	ld1	{ v3.b }[1], [x10]
	add	x10, sp, #848
	ld1	{ v4.b }[14], [x8]
	add	x8, sp, #1104
	ld1	{ v5.b }[14], [x9]
	add	x9, sp, #1224
	sdot	v6.4s, v19.16b, v18.16b
	ld1	{ v16.b }[2], [x8]
	add	x8, sp, #720
	ld1	{ v2.b }[1], [x9]
	add	x9, sp, #1232
	ld1	{ v3.b }[2], [x10]
	add	x10, sp, #856
	ld1	{ v17.b }[2], [x8]
	add	x8, sp, #728
	ld1	{ v16.b }[3], [x11]
	add	x11, sp, #1120
	ld1	{ v2.b }[2], [x9]
	add	x9, sp, #1240
	ld1	{ v3.b }[3], [x10]
	add	x10, sp, #864
	ld1	{ v17.b }[3], [x8]
	add	x8, sp, #736
	ld1	{ v16.b }[4], [x11]
	add	x11, sp, #1128
	ld1	{ v2.b }[3], [x9]
	add	x9, sp, #1248
	ld1	{ v3.b }[4], [x10]
	add	x10, sp, #872
	ld1	{ v17.b }[4], [x8]
	add	x8, sp, #744
	ld1	{ v16.b }[5], [x11]
	add	x11, sp, #1136
	ld1	{ v2.b }[4], [x9]
	add	x9, sp, #1256
	ld1	{ v3.b }[5], [x10]
	add	x10, sp, #880
	ld1	{ v17.b }[5], [x8]
	add	x8, sp, #752
	ld1	{ v16.b }[6], [x11]
	add	x11, sp, #1144
	ld1	{ v2.b }[5], [x9]
	add	x9, sp, #1264
	ld1	{ v3.b }[6], [x10]
	add	x10, sp, #888
	ld1	{ v17.b }[6], [x8]
	add	x8, sp, #760
	ld1	{ v16.b }[7], [x11]
	add	x11, sp, #1152
	ld1	{ v2.b }[6], [x9]
	add	x9, sp, #1272
	ld1	{ v3.b }[7], [x10]
	add	x10, sp, #896
	ld1	{ v17.b }[7], [x8]
	add	x8, sp, #768
	ld1	{ v16.b }[8], [x11]
	add	x11, sp, #1160
	ld1	{ v2.b }[7], [x9]
	add	x9, sp, #1280
	ld1	{ v3.b }[8], [x10]
	add	x10, sp, #904
	ld1	{ v17.b }[8], [x8]
	add	x8, sp, #776
	ld1	{ v16.b }[9], [x11]
	add	x11, sp, #1168
	ld1	{ v2.b }[8], [x9]
	add	x9, sp, #1288
	ld1	{ v3.b }[9], [x10]
	add	x10, sp, #912
	ld1	{ v17.b }[9], [x8]
	add	x8, sp, #784
	ld1	{ v16.b }[10], [x11]
	add	x11, sp, #1176
	ld1	{ v2.b }[9], [x9]
	add	x9, sp, #1296
	ld1	{ v3.b }[10], [x10]
	add	x10, sp, #920
	ld1	{ v17.b }[10], [x8]
	add	x8, sp, #792
	ld1	{ v16.b }[11], [x11]
	add	x11, sp, #1184
	ld1	{ v2.b }[10], [x9]
	add	x9, sp, #1304
	ld1	{ v3.b }[11], [x10]
	add	x10, sp, #928
	ld1	{ v17.b }[11], [x8]
	add	x8, sp, #800
	ld1	{ v16.b }[12], [x11]
	add	x11, sp, #1192
	ld1	{ v2.b }[11], [x9]
	add	x9, sp, #1312
	ld1	{ v3.b }[12], [x10]
	add	x10, sp, #936
	ld1	{ v17.b }[12], [x8]
	add	x8, sp, #808
	ld1	{ v16.b }[13], [x11]
	add	x11, sp, #1200
	ld1	{ v2.b }[12], [x9]
	add	x9, sp, #1320
	ld1	{ v3.b }[13], [x10]
	add	x10, sp, #944
	ld1	{ v17.b }[13], [x8]
	add	x8, sp, #816
	ld1	{ v16.b }[14], [x11]
	add	x11, sp, #1208
	ld1	{ v2.b }[13], [x9]
	add	x9, sp, #1328
	ld1	{ v3.b }[14], [x10]
	add	x10, sp, #952
	ld1	{ v17.b }[14], [x8]
	add	x8, sp, #824
	ld1	{ v16.b }[15], [x11]
	add	x11, sp, #568
	ld1	{ v2.b }[14], [x9]
	add	x9, sp, #1336
	sdot	v7.4s, v1.16b, v0.16b
	ld1	{ v3.b }[15], [x10]
	ld1	{ v17.b }[15], [x8]
	add	x8, sp, #184
	ld1	{ v4.b }[15], [x11]
	ld1	{ v2.b }[15], [x9]
	ld1	{ v5.b }[15], [x8]
	sdot	v6.4s, v17.16b, v16.16b
	sdot	v7.4s, v5.16b, v4.16b
	sdot	v6.4s, v3.16b, v2.16b
	add	v0.4s, v7.4s, v6.4s
	addv	s0, v0.4s
	fmov	w0, s0
	ret
.Lfunc_end49:
	.size	test_sdot_v48i8_double, .Lfunc_end49-test_sdot_v48i8_double
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v48i8_double_nomla    // -- Begin function test_sdot_v48i8_double_nomla
	.p2align	2
	.type	test_sdot_v48i8_double_nomla,@function
test_sdot_v48i8_double_nomla:           // @test_sdot_v48i8_double_nomla
	.cfi_startproc
// %bb.0:                               // %entry
	ldr	b0, [sp, #192]
	add	x8, sp, #200
	fmov	s1, w0
	add	x9, sp, #216
	ldr	b2, [sp, #64]
	add	x11, sp, #72
	ld1	{ v0.b }[1], [x8]
	add	x8, sp, #208
	mov	v1.b[1], w1
	add	x10, sp, #232
	ld1	{ v2.b }[1], [x11]
	add	x11, sp, #712
	ldr	b4, [sp, #704]
	add	x12, sp, #968
	ld1	{ v0.b }[2], [x8]
	add	x8, sp, #224
	mov	v1.b[2], w2
	ldr	b3, [sp, #960]
	ldr	b5, [sp, #832]
	add	x13, sp, #80
	ld1	{ v4.b }[1], [x11]
	add	x11, sp, #840
	ld1	{ v0.b }[3], [x9]
	add	x9, sp, #240
	mov	v1.b[3], w3
	ld1	{ v3.b }[1], [x12]
	add	x12, sp, #248
	ld1	{ v5.b }[1], [x11]
	add	x11, sp, #976
	ld1	{ v2.b }[2], [x13]
	ld1	{ v0.b }[4], [x8]
	add	x13, sp, #720
	mov	v1.b[4], w4
	add	x8, sp, #256
	ld1	{ v3.b }[2], [x11]
	add	x14, sp, #848
	ld1	{ v4.b }[2], [x13]
	add	x13, sp, #984
	ld1	{ v0.b }[5], [x10]
	add	x11, sp, #88
	mov	v1.b[5], w5
	add	x10, sp, #264
	ld1	{ v3.b }[3], [x13]
	mov	x13, sp
	ld1	{ v5.b }[2], [x14]
	add	x14, sp, #280
	ld1	{ v0.b }[6], [x9]
	add	x9, sp, #272
	mov	v1.b[6], w6
	ld1	{ v2.b }[3], [x11]
	add	x11, sp, #856
	movi	v6.16b, #1
	ld1	{ v0.b }[7], [x12]
	add	x12, sp, #728
	mov	v1.b[7], w7
	ld1	{ v5.b }[3], [x11]
	add	x11, sp, #992
	ld1	{ v4.b }[3], [x12]
	add	x12, sp, #96
	ld1	{ v0.b }[8], [x8]
	add	x8, sp, #288
	ld1	{ v1.b }[8], [x13]
	add	x13, sp, #8
	ld1	{ v3.b }[4], [x11]
	add	x11, sp, #864
	ld1	{ v2.b }[4], [x12]
	add	x12, sp, #736
	ld1	{ v0.b }[9], [x10]
	add	x10, sp, #296
	ld1	{ v1.b }[9], [x13]
	add	x13, sp, #16
	ld1	{ v5.b }[4], [x11]
	add	x11, sp, #1000
	ld1	{ v4.b }[4], [x12]
	add	x12, sp, #104
	ld1	{ v0.b }[10], [x9]
	add	x9, sp, #304
	ld1	{ v1.b }[10], [x13]
	add	x13, sp, #24
	ld1	{ v3.b }[5], [x11]
	add	x11, sp, #872
	ld1	{ v2.b }[5], [x12]
	add	x12, sp, #744
	ld1	{ v0.b }[11], [x14]
	add	x14, sp, #312
	ld1	{ v1.b }[11], [x13]
	add	x13, sp, #32
	ld1	{ v4.b }[5], [x12]
	add	x12, sp, #40
	ld1	{ v5.b }[5], [x11]
	add	x11, sp, #112
	ld1	{ v0.b }[12], [x8]
	add	x8, sp, #1008
	ld1	{ v1.b }[12], [x13]
	ld1	{ v2.b }[6], [x11]
	add	x11, sp, #48
	ld1	{ v3.b }[6], [x8]
	add	x8, sp, #1016
	ld1	{ v0.b }[13], [x10]
	add	x10, sp, #752
	ld1	{ v1.b }[13], [x12]
	movi	v7.2d, #0000000000000000
	ld1	{ v3.b }[7], [x8]
	add	x8, sp, #1024
	ld1	{ v4.b }[6], [x10]
	add	x10, sp, #880
	ld1	{ v0.b }[14], [x9]
	add	x9, sp, #760
	ld1	{ v1.b }[14], [x11]
	add	x11, sp, #120
	ld1	{ v3.b }[8], [x8]
	add	x8, sp, #1032
	ld1	{ v4.b }[7], [x9]
	add	x9, sp, #768
	ld1	{ v5.b }[6], [x10]
	add	x10, sp, #888
	ld1	{ v2.b }[7], [x11]
	add	x11, sp, #128
	ld1	{ v3.b }[9], [x8]
	add	x8, sp, #1040
	ld1	{ v4.b }[8], [x9]
	add	x9, sp, #776
	ld1	{ v5.b }[7], [x10]
	add	x10, sp, #896
	ld1	{ v2.b }[8], [x11]
	add	x11, sp, #136
	ld1	{ v3.b }[10], [x8]
	add	x8, sp, #1048
	ld1	{ v4.b }[9], [x9]
	add	x9, sp, #784
	ld1	{ v5.b }[8], [x10]
	add	x10, sp, #904
	ld1	{ v2.b }[9], [x11]
	add	x11, sp, #144
	ld1	{ v3.b }[11], [x8]
	add	x8, sp, #1056
	ld1	{ v4.b }[10], [x9]
	add	x9, sp, #792
	ld1	{ v5.b }[9], [x10]
	add	x10, sp, #912
	ld1	{ v2.b }[10], [x11]
	add	x11, sp, #152
	ld1	{ v3.b }[12], [x8]
	add	x8, sp, #1064
	ld1	{ v4.b }[11], [x9]
	add	x9, sp, #800
	ld1	{ v5.b }[10], [x10]
	add	x10, sp, #920
	ld1	{ v2.b }[11], [x11]
	add	x11, sp, #160
	ld1	{ v3.b }[13], [x8]
	add	x8, sp, #1072
	ld1	{ v4.b }[12], [x9]
	add	x9, sp, #808
	ld1	{ v5.b }[11], [x10]
	add	x10, sp, #928
	ld1	{ v2.b }[12], [x11]
	add	x11, sp, #168
	ld1	{ v3.b }[14], [x8]
	add	x8, sp, #1080
	ld1	{ v4.b }[13], [x9]
	add	x9, sp, #816
	ld1	{ v5.b }[12], [x10]
	add	x10, sp, #936
	movi	v16.2d, #0000000000000000
	ld1	{ v0.b }[15], [x14]
	ld1	{ v2.b }[13], [x11]
	add	x11, sp, #56
	ld1	{ v3.b }[15], [x8]
	add	x8, sp, #176
	ld1	{ v4.b }[14], [x9]
	add	x9, sp, #824
	ld1	{ v5.b }[13], [x10]
	add	x10, sp, #944
	ld1	{ v1.b }[15], [x11]
	sdot	v16.4s, v0.16b, v6.16b
	ld1	{ v2.b }[14], [x8]
	sdot	v7.4s, v3.16b, v6.16b
	ld1	{ v4.b }[15], [x9]
	ld1	{ v5.b }[14], [x10]
	add	x8, sp, #184
	add	x9, sp, #952
	sdot	v16.4s, v1.16b, v6.16b
	ld1	{ v2.b }[15], [x8]
	sdot	v7.4s, v4.16b, v6.16b
	ld1	{ v5.b }[15], [x9]
	sdot	v16.4s, v2.16b, v6.16b
	sdot	v7.4s, v5.16b, v6.16b
	add	v0.4s, v16.4s, v7.4s
	addv	s0, v0.4s
	fmov	w0, s0
	ret
.Lfunc_end50:
	.size	test_sdot_v48i8_double_nomla, .Lfunc_end50-test_sdot_v48i8_double_nomla
	.cfi_endproc
                                        // -- End function
	.globl	test_udot_v64i8                 // -- Begin function test_udot_v64i8
	.p2align	2
	.type	test_udot_v64i8,@function
test_udot_v64i8:                        // @test_udot_v64i8
	.cfi_startproc
// %bb.0:                               // %entry
	ldp	q1, q4, [x1, #32]
	movi	v5.2d, #0000000000000000
	movi	v0.2d, #0000000000000000
	ldp	q2, q3, [x0, #32]
	udot	v5.4s, v1.16b, v2.16b
	ldp	q6, q7, [x0]
	udot	v0.4s, v4.16b, v3.16b
	ldp	q1, q16, [x1]
	udot	v5.4s, v1.16b, v6.16b
	udot	v0.4s, v16.16b, v7.16b
	add	v0.4s, v5.4s, v0.4s
	addv	s0, v0.4s
	fmov	w8, s0
	add	w0, w8, w2
	ret
.Lfunc_end51:
	.size	test_udot_v64i8, .Lfunc_end51-test_udot_v64i8
	.cfi_endproc
                                        // -- End function
	.globl	test_udot_v64i8_nomla           // -- Begin function test_udot_v64i8_nomla
	.p2align	2
	.type	test_udot_v64i8_nomla,@function
test_udot_v64i8_nomla:                  // @test_udot_v64i8_nomla
	.cfi_startproc
// %bb.0:                               // %entry
	ldp	q4, q3, [x0, #32]
	movi	v0.16b, #1
	movi	v1.2d, #0000000000000000
	movi	v2.2d, #0000000000000000
	udot	v1.4s, v3.16b, v0.16b
	ldp	q3, q5, [x0]
	udot	v2.4s, v4.16b, v0.16b
	udot	v2.4s, v3.16b, v0.16b
	udot	v1.4s, v5.16b, v0.16b
	add	v0.4s, v2.4s, v1.4s
	addv	s0, v0.4s
	fmov	w0, s0
	ret
.Lfunc_end52:
	.size	test_udot_v64i8_nomla, .Lfunc_end52-test_udot_v64i8_nomla
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v64i8                 // -- Begin function test_sdot_v64i8
	.p2align	2
	.type	test_sdot_v64i8,@function
test_sdot_v64i8:                        // @test_sdot_v64i8
	.cfi_startproc
// %bb.0:                               // %entry
	ldp	q1, q4, [x1, #32]
	movi	v5.2d, #0000000000000000
	movi	v0.2d, #0000000000000000
	ldp	q2, q3, [x0, #32]
	sdot	v5.4s, v1.16b, v2.16b
	ldp	q6, q7, [x0]
	sdot	v0.4s, v4.16b, v3.16b
	ldp	q1, q16, [x1]
	sdot	v5.4s, v1.16b, v6.16b
	sdot	v0.4s, v16.16b, v7.16b
	add	v0.4s, v5.4s, v0.4s
	addv	s0, v0.4s
	fmov	w8, s0
	add	w0, w8, w2
	ret
.Lfunc_end53:
	.size	test_sdot_v64i8, .Lfunc_end53-test_sdot_v64i8
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v64i8_double          // -- Begin function test_sdot_v64i8_double
	.p2align	2
	.type	test_sdot_v64i8_double,@function
test_sdot_v64i8_double:                 // @test_sdot_v64i8_double
	.cfi_startproc
// %bb.0:                               // %entry
	ldp	q18, q19, [sp, #96]
	movi	v22.2d, #0000000000000000
	movi	v23.2d, #0000000000000000
	movi	v24.2d, #0000000000000000
	movi	v25.2d, #0000000000000000
	sdot	v22.4s, v3.16b, v7.16b
	ldp	q20, q21, [sp, #32]
	sdot	v23.4s, v2.16b, v6.16b
	sdot	v22.4s, v1.16b, v5.16b
	sdot	v25.4s, v20.16b, v18.16b
	sdot	v23.4s, v0.16b, v4.16b
	ldp	q16, q17, [sp, #64]
	sdot	v24.4s, v21.16b, v19.16b
	add	v0.4s, v23.4s, v22.4s
	ldp	q26, q3, [sp]
	sdot	v25.4s, v26.16b, v16.16b
	sdot	v24.4s, v3.16b, v17.16b
	add	v1.4s, v25.4s, v24.4s
	add	v0.4s, v0.4s, v1.4s
	addv	s0, v0.4s
	fmov	w0, s0
	ret
.Lfunc_end54:
	.size	test_sdot_v64i8_double, .Lfunc_end54-test_sdot_v64i8_double
	.cfi_endproc
                                        // -- End function
	.globl	test_sdot_v64i8_double_nomla    // -- Begin function test_sdot_v64i8_double_nomla
	.p2align	2
	.type	test_sdot_v64i8_double_nomla,@function
test_sdot_v64i8_double_nomla:           // @test_sdot_v64i8_double_nomla
	.cfi_startproc
// %bb.0:                               // %entry
	ldp	q4, q5, [sp, #32]
	movi	v6.16b, #1
	movi	v7.2d, #0000000000000000
	movi	v16.2d, #0000000000000000
	movi	v17.2d, #0000000000000000
	movi	v18.2d, #0000000000000000
	sdot	v7.4s, v3.16b, v6.16b
	sdot	v16.4s, v2.16b, v6.16b
	ldp	q3, q2, [sp]
	sdot	v17.4s, v5.16b, v6.16b
	sdot	v18.4s, v4.16b, v6.16b
	sdot	v7.4s, v1.16b, v6.16b
	sdot	v16.4s, v0.16b, v6.16b
	sdot	v17.4s, v2.16b, v6.16b
	sdot	v18.4s, v3.16b, v6.16b
	add	v0.4s, v16.4s, v7.4s
	add	v1.4s, v18.4s, v17.4s
	add	v0.4s, v0.4s, v1.4s
	addv	s0, v0.4s
	fmov	w0, s0
	ret
.Lfunc_end55:
	.size	test_sdot_v64i8_double_nomla, .Lfunc_end55-test_sdot_v64i8_double_nomla
	.cfi_endproc
                                        // -- End function
	.section	".note.GNU-stack","",@progbits
