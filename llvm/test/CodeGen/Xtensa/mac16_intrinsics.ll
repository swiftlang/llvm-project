; RUN: llc -O1 -mtriple=xtensa -mcpu=esp32 %s -o - | FileCheck %s

define void @test_xtensa_umul(i32 %a, i32 %b) nounwind {
; CHECK-LABEL: test_xtensa_umul
; CHECK: umul.aa.ll	a2, a3
  call void @llvm.xtensa.umul.aa.ll(i32 %a, i32 %b)
; CHECK: umul.aa.lh	a2, a3
  call void @llvm.xtensa.umul.aa.lh(i32 %a, i32 %b)
; CHECK: umul.aa.hl	a2, a3
  call void @llvm.xtensa.umul.aa.hl(i32 %a, i32 %b)
; CHECK: umul.aa.hh	a2, a3
  call void @llvm.xtensa.umul.aa.hh(i32 %a, i32 %b)
  ret void
}

define void @test_xtensa_mul(i32 %a, i32 %b) nounwind {
; CHECK-LABEL: test_xtensa_mul
; CHECK: mul.aa.ll	a2, a3
  call void @llvm.xtensa.mul.aa.ll(i32 %a, i32 %b)
; CHECK: mul.aa.lh	a2, a3
  call void @llvm.xtensa.mul.aa.lh(i32 %a, i32 %b)
; CHECK: mul.aa.hl	a2, a3
  call void @llvm.xtensa.mul.aa.hl(i32 %a, i32 %b)
; CHECK: mul.aa.hh	a2, a3
  call void @llvm.xtensa.mul.aa.hh(i32 %a, i32 %b)
; CHECK: mul.ad.ll	a2, m2
  call void @llvm.xtensa.mul.ad.ll(i32 %a, i32 2)
; CHECK: mul.ad.lh	a2, m2
  call void @llvm.xtensa.mul.ad.lh(i32 %a, i32 2)
; CHECK: mul.ad.hl	a2, m2
  call void @llvm.xtensa.mul.ad.hl(i32 %a, i32 2)
; CHECK: mul.ad.hh	a2, m2
  call void @llvm.xtensa.mul.ad.hh(i32 %a, i32 2)
; CHECK: mul.da.ll	m1, a3
  call void @llvm.xtensa.mul.da.ll(i32 1, i32 %b)
; CHECK: mul.da.lh	m1, a3
  call void @llvm.xtensa.mul.da.lh(i32 1, i32 %b)
; CHECK: mul.da.hl	m1, a3
  call void @llvm.xtensa.mul.da.hl(i32 1, i32 %b)
; CHECK: mul.da.hh	m1, a3
  call void @llvm.xtensa.mul.da.hh(i32 1, i32 %b)
; CHECK: mul.dd.ll	m1, m2
  call void @llvm.xtensa.mul.dd.ll(i32 1, i32 2)
; CHECK: mul.dd.lh	m1, m2
  call void @llvm.xtensa.mul.dd.lh(i32 1, i32 2)
; CHECK: mul.dd.hl	m1, m2
  call void @llvm.xtensa.mul.dd.hl(i32 1, i32 2)
; CHECK: mul.dd.hh	m1, m2
  call void @llvm.xtensa.mul.dd.hh(i32 1, i32 2)
  ret void
}

define void @test_xtensa_mula(i32 %a, i32 %b) nounwind {
; CHECK-LABEL: test_xtensa_mula
; CHECK: mula.aa.ll	a2, a3
  call void @llvm.xtensa.mula.aa.ll(i32 %a, i32 %b)
; CHECK: mula.aa.lh	a2, a3
  call void @llvm.xtensa.mula.aa.lh(i32 %a, i32 %b)
; CHECK: mula.aa.hl	a2, a3
  call void @llvm.xtensa.mula.aa.hl(i32 %a, i32 %b)
; CHECK: mula.aa.hh	a2, a3
  call void @llvm.xtensa.mula.aa.hh(i32 %a, i32 %b)
; CHECK: mula.ad.ll	a2, m2
  call void @llvm.xtensa.mula.ad.ll(i32 %a, i32 2)
; CHECK: mula.ad.lh	a2, m2
  call void @llvm.xtensa.mula.ad.lh(i32 %a, i32 2)
; CHECK: mula.ad.hl	a2, m2
  call void @llvm.xtensa.mula.ad.hl(i32 %a, i32 2)
; CHECK: mula.ad.hh	a2, m2
  call void @llvm.xtensa.mula.ad.hh(i32 %a, i32 2)
; CHECK: mula.da.ll	m1, a3
  call void @llvm.xtensa.mula.da.ll(i32 1, i32 %b)
; CHECK: mula.da.lh	m1, a3
  call void @llvm.xtensa.mula.da.lh(i32 1, i32 %b)
; CHECK: mula.da.hl	m1, a3
  call void @llvm.xtensa.mula.da.hl(i32 1, i32 %b)
; CHECK: mula.da.hh	m1, a3
  call void @llvm.xtensa.mula.da.hh(i32 1, i32 %b)
; CHECK: mula.dd.ll	m1, m2
  call void @llvm.xtensa.mula.dd.ll(i32 1, i32 2)
; CHECK: mula.dd.lh	m1, m2
  call void @llvm.xtensa.mula.dd.lh(i32 1, i32 2)
; CHECK: mula.dd.hl	m1, m2
  call void @llvm.xtensa.mula.dd.hl(i32 1, i32 2)
; CHECK: mula.dd.hh	m1, m2
  call void @llvm.xtensa.mula.dd.hh(i32 1, i32 2)
  ret void
}

define void @test_xtensa_muls(i32 %a, i32 %b) nounwind {
; CHECK-LABEL: test_xtensa_muls
; CHECK: muls.aa.ll	a2, a3
  call void @llvm.xtensa.muls.aa.ll(i32 %a, i32 %b)
; CHECK: muls.aa.lh	a2, a3
  call void @llvm.xtensa.muls.aa.lh(i32 %a, i32 %b)
; CHECK: muls.aa.hl	a2, a3
  call void @llvm.xtensa.muls.aa.hl(i32 %a, i32 %b)
; CHECK: muls.aa.hh	a2, a3
  call void @llvm.xtensa.muls.aa.hh(i32 %a, i32 %b)
; CHECK: muls.ad.ll	a2, m2
  call void @llvm.xtensa.muls.ad.ll(i32 %a, i32 2)
; CHECK: muls.ad.lh	a2, m2
  call void @llvm.xtensa.muls.ad.lh(i32 %a, i32 2)
; CHECK: muls.ad.hl	a2, m2
  call void @llvm.xtensa.muls.ad.hl(i32 %a, i32 2)
; CHECK: muls.ad.hh	a2, m2
  call void @llvm.xtensa.muls.ad.hh(i32 %a, i32 2)
; CHECK: muls.da.ll	m1, a3
  call void @llvm.xtensa.muls.da.ll(i32 1, i32 %b)
; CHECK: muls.da.lh	m1, a3
  call void @llvm.xtensa.muls.da.lh(i32 1, i32 %b)
; CHECK: muls.da.hl	m1, a3
  call void @llvm.xtensa.muls.da.hl(i32 1, i32 %b)
; CHECK: muls.da.hh	m1, a3
  call void @llvm.xtensa.muls.da.hh(i32 1, i32 %b)
; CHECK: muls.dd.ll	m1, m2
  call void @llvm.xtensa.muls.dd.ll(i32 1, i32 2)
; CHECK: muls.dd.lh	m1, m2
  call void @llvm.xtensa.muls.dd.lh(i32 1, i32 2)
; CHECK: muls.dd.hl	m1, m2
  call void @llvm.xtensa.muls.dd.hl(i32 1, i32 2)
; CHECK: muls.dd.hh	m1, m2
  call void @llvm.xtensa.muls.dd.hh(i32 1, i32 2)
  ret void
}

define void @test_xtensa_mula_ld(i32 %pa.coerce, i32 %b) nounwind {
; CHECK-LABEL: test_xtensa_mula_ld
entry:
  %0 = inttoptr i32 %pa.coerce to i8*
; CHECK: 	mula.da.ll.lddec	 m1, a{{[0-9]+}}, m0, a3
  call void @llvm.xtensa.mula.da.ll.lddec(i32 1, i8* %0, i32 0, i32 %b)
; CHECK: 	mula.da.lh.lddec	 m1, a{{[0-9]+}}, m0, a3
  call void @llvm.xtensa.mula.da.lh.lddec(i32 1, i8* %0, i32 0, i32 %b)
; CHECK: 	mula.da.hl.lddec	 m1, a{{[0-9]+}}, m0, a3
  call void @llvm.xtensa.mula.da.hl.lddec(i32 1, i8* %0, i32 0, i32 %b)
; CHECK: 	mula.da.hh.lddec	 m1, a{{[0-9]+}}, m0, a3
  call void @llvm.xtensa.mula.da.hh.lddec(i32 1, i8* %0, i32 0, i32 %b)
; CHECK: 	mula.dd.ll.lddec	 m1, a{{[0-9]+}}, m0, m2
  call void @llvm.xtensa.mula.dd.ll.lddec(i32 1, i8* %0, i32 0, i32 2)
; CHECK: 	mula.dd.lh.lddec	 m1, a{{[0-9]+}}, m0, m2
  call void @llvm.xtensa.mula.dd.lh.lddec(i32 1, i8* %0, i32 0, i32 2)
; CHECK: 	mula.dd.hl.lddec	 m1, a{{[0-9]+}}, m0, m2
  call void @llvm.xtensa.mula.dd.hl.lddec(i32 1, i8* %0, i32 0, i32 2)
; CHECK: 	mula.dd.hh.lddec	 m1, a{{[0-9]+}}, m0, m2
  call void @llvm.xtensa.mula.dd.hh.lddec(i32 1, i8* %0, i32 0, i32 2)
; CHECK: 	mula.da.ll.ldinc	 m1, a{{[0-9]+}}, m0, a3
  call void @llvm.xtensa.mula.da.ll.ldinc(i32 1, i8* %0, i32 0, i32 %b)
; CHECK: 	mula.da.lh.ldinc	 m1, a{{[0-9]+}}, m0, a3
  call void @llvm.xtensa.mula.da.lh.ldinc(i32 1, i8* %0, i32 0, i32 %b)
; CHECK: 	mula.da.hl.ldinc	 m1, a{{[0-9]+}}, m0, a3
  call void @llvm.xtensa.mula.da.hl.ldinc(i32 1, i8* %0, i32 0, i32 %b)
; CHECK: 	mula.da.hh.ldinc	 m1, a{{[0-9]+}}, m0, a3
  call void @llvm.xtensa.mula.da.hh.ldinc(i32 1, i8* %0, i32 0, i32 %b)
; CHECK: 	mula.dd.ll.ldinc	 m1, a{{[0-9]+}}, m0, m2
  call void @llvm.xtensa.mula.dd.ll.ldinc(i32 1, i8* %0, i32 0, i32 2)
; CHECK: 	mula.dd.lh.ldinc	 m1, a{{[0-9]+}}, m0, m2
  call void @llvm.xtensa.mula.dd.lh.ldinc(i32 1, i8* %0, i32 0, i32 2)
; CHECK: 	mula.dd.hl.ldinc	 m1, a{{[0-9]+}}, m0, m2
  call void @llvm.xtensa.mula.dd.hl.ldinc(i32 1, i8* %0, i32 0, i32 2)
; CHECK:	mula.dd.hh.ldinc	 m1, a{{[0-9]+}}, m0, m2
  call void @llvm.xtensa.mula.dd.hh.ldinc(i32 1, i8* %0, i32 0, i32 2)
  ret void
}

define void @test_xtensa_ld(i32 %pa.coerce) nounwind {
; CHECK-LABEL: test_xtensa_ld
entry:
  %0 = inttoptr i32 %pa.coerce to i8*
; CHECK:	lddec	 m0, a{{[0-9]+}}
  call void @llvm.xtensa.lddec(i32 0, i8* %0)
; CHECK:	ldinc	 m0, a{{[0-9]+}}
  call void @llvm.xtensa.ldinc(i32 0, i8* %0)
  ret void
}

define void @test_xtensa_wsr(i32 %a) {
; CHECK-LABEL: test_xtensa_wsr
; CHECK: wsr	a2, acclo
  call void @llvm.xtensa.wsr.acclo(i32 %a)
; CHECK: wsr	a2, acchi
  call void @llvm.xtensa.wsr.acchi(i32 %a)
; CHECK: wsr	a2, m0
  call void @llvm.xtensa.wsr.m0(i32 %a)
; CHECK: wsr	a2, m1
  call void @llvm.xtensa.wsr.m1(i32 %a)
; CHECK: wsr	a2, m2
  call void @llvm.xtensa.wsr.m2(i32 %a)
; CHECK: wsr	a2, m3
  call void @llvm.xtensa.wsr.m3(i32 %a)
  ret void
}

define void @test_xtensa_xsr(i32 %a.coerce) {
; CHECK-LABEL: test_xtensa_xsr
entry:
  %0 = inttoptr i32 %a.coerce to i8*
; CHECK: xsr	a{{[0-9]+}}, acclo
  call void @llvm.xtensa.xsr.acclo(i8* %0)
; CHECK: xsr	a{{[0-9]+}}, acchi
  call void @llvm.xtensa.xsr.acchi(i8* %0)
; CHECK: xsr	a{{[0-9]+}}, m0
  call void @llvm.xtensa.xsr.m0(i8* %0)
; CHECK: xsr	a{{[0-9]+}}, m1
  call void @llvm.xtensa.xsr.m1(i8* %0)
; CHECK: xsr	a{{[0-9]+}}, m2
  call void @llvm.xtensa.xsr.m2(i8* %0)
; CHECK: xsr	a{{[0-9]+}}, m3
  call void @llvm.xtensa.xsr.m3(i8* %0)
  ret void
}

define void @test_xtensa_rsr() {
; CHECK-LABEL: test_xtensa_rsr
entry:
; CHECK: rsr	a{{[0-9]+}}, acclo
  %0 = call i32 @llvm.xtensa.rsr.acclo()
; CHECK: rsr	a{{[0-9]+}}, acchi
  %1 = call i32 @llvm.xtensa.rsr.acchi()
; CHECK: rsr	a{{[0-9]+}}, m0
  %2 = call i32 @llvm.xtensa.rsr.m0()
; CHECK: rsr	a{{[0-9]+}}, m1
  %3 = call i32 @llvm.xtensa.rsr.m1()
; CHECK: rsr	a{{[0-9]+}}, m2
  %4 = call i32 @llvm.xtensa.rsr.m2()
; CHECK: rsr	a{{[0-9]+}}, m3
  %5 = call i32 @llvm.xtensa.rsr.m3()
  ret void
}

declare void @llvm.xtensa.umul.aa.ll(i32, i32) nounwind
declare void @llvm.xtensa.umul.aa.lh(i32, i32) nounwind
declare void @llvm.xtensa.umul.aa.hl(i32, i32) nounwind
declare void @llvm.xtensa.umul.aa.hh(i32, i32) nounwind
declare void @llvm.xtensa.mul.aa.ll(i32, i32) nounwind
declare void @llvm.xtensa.mul.aa.lh(i32, i32) nounwind
declare void @llvm.xtensa.mul.aa.hl(i32, i32) nounwind
declare void @llvm.xtensa.mul.aa.hh(i32, i32) nounwind
declare void @llvm.xtensa.mul.ad.ll(i32, i32 immarg) nounwind
declare void @llvm.xtensa.mul.ad.lh(i32, i32 immarg) nounwind
declare void @llvm.xtensa.mul.ad.hl(i32, i32 immarg) nounwind
declare void @llvm.xtensa.mul.ad.hh(i32, i32 immarg) nounwind
declare void @llvm.xtensa.mul.da.ll(i32 immarg, i32) nounwind
declare void @llvm.xtensa.mul.da.lh(i32 immarg, i32) nounwind
declare void @llvm.xtensa.mul.da.hl(i32 immarg, i32) nounwind
declare void @llvm.xtensa.mul.da.hh(i32 immarg, i32) nounwind
declare void @llvm.xtensa.mul.dd.ll(i32 immarg, i32 immarg) nounwind
declare void @llvm.xtensa.mul.dd.lh(i32 immarg, i32 immarg) nounwind
declare void @llvm.xtensa.mul.dd.hl(i32 immarg, i32 immarg) nounwind
declare void @llvm.xtensa.mul.dd.hh(i32 immarg, i32 immarg) nounwind
declare void @llvm.xtensa.mula.aa.ll(i32, i32) nounwind
declare void @llvm.xtensa.mula.aa.lh(i32, i32) nounwind
declare void @llvm.xtensa.mula.aa.hl(i32, i32) nounwind
declare void @llvm.xtensa.mula.aa.hh(i32, i32) nounwind
declare void @llvm.xtensa.mula.ad.ll(i32, i32 immarg) nounwind
declare void @llvm.xtensa.mula.ad.lh(i32, i32 immarg) nounwind
declare void @llvm.xtensa.mula.ad.hl(i32, i32 immarg) nounwind
declare void @llvm.xtensa.mula.ad.hh(i32, i32 immarg) nounwind
declare void @llvm.xtensa.mula.da.ll(i32 immarg, i32) nounwind
declare void @llvm.xtensa.mula.da.lh(i32 immarg, i32) nounwind
declare void @llvm.xtensa.mula.da.hl(i32 immarg, i32) nounwind
declare void @llvm.xtensa.mula.da.hh(i32 immarg, i32) nounwind
declare void @llvm.xtensa.mula.dd.ll(i32 immarg, i32 immarg) nounwind
declare void @llvm.xtensa.mula.dd.lh(i32 immarg, i32 immarg) nounwind
declare void @llvm.xtensa.mula.dd.hl(i32 immarg, i32 immarg) nounwind
declare void @llvm.xtensa.mula.dd.hh(i32 immarg, i32 immarg) nounwind
declare void @llvm.xtensa.muls.aa.ll(i32, i32) nounwind
declare void @llvm.xtensa.muls.aa.lh(i32, i32) nounwind
declare void @llvm.xtensa.muls.aa.hl(i32, i32) nounwind
declare void @llvm.xtensa.muls.aa.hh(i32, i32) nounwind
declare void @llvm.xtensa.muls.ad.ll(i32, i32 immarg) nounwind
declare void @llvm.xtensa.muls.ad.lh(i32, i32 immarg) nounwind
declare void @llvm.xtensa.muls.ad.hl(i32, i32 immarg) nounwind
declare void @llvm.xtensa.muls.ad.hh(i32, i32 immarg) nounwind
declare void @llvm.xtensa.muls.da.ll(i32 immarg, i32) nounwind
declare void @llvm.xtensa.muls.da.lh(i32 immarg, i32) nounwind
declare void @llvm.xtensa.muls.da.hl(i32 immarg, i32) nounwind
declare void @llvm.xtensa.muls.da.hh(i32 immarg, i32) nounwind
declare void @llvm.xtensa.muls.dd.ll(i32 immarg, i32 immarg) nounwind
declare void @llvm.xtensa.muls.dd.lh(i32 immarg, i32 immarg) nounwind
declare void @llvm.xtensa.muls.dd.hl(i32 immarg, i32 immarg) nounwind
declare void @llvm.xtensa.muls.dd.hh(i32 immarg, i32 immarg) nounwind
declare void @llvm.xtensa.mula.da.ll.lddec(i32 immarg, i8*, i32 immarg, i32) nounwind
declare void @llvm.xtensa.mula.da.lh.lddec(i32 immarg, i8*, i32 immarg, i32) nounwind
declare void @llvm.xtensa.mula.da.hl.lddec(i32 immarg, i8*, i32 immarg, i32) nounwind
declare void @llvm.xtensa.mula.da.hh.lddec(i32 immarg, i8*, i32 immarg, i32) nounwind
declare void @llvm.xtensa.mula.dd.ll.lddec(i32 immarg, i8*, i32 immarg, i32 immarg) nounwind
declare void @llvm.xtensa.mula.dd.lh.lddec(i32 immarg, i8*, i32 immarg, i32 immarg) nounwind
declare void @llvm.xtensa.mula.dd.hl.lddec(i32 immarg, i8*, i32 immarg, i32 immarg) nounwind
declare void @llvm.xtensa.mula.dd.hh.lddec(i32 immarg, i8*, i32 immarg, i32 immarg) nounwind
declare void @llvm.xtensa.mula.da.ll.ldinc(i32 immarg, i8*, i32 immarg, i32) nounwind
declare void @llvm.xtensa.mula.da.lh.ldinc(i32 immarg, i8*, i32 immarg, i32) nounwind
declare void @llvm.xtensa.mula.da.hl.ldinc(i32 immarg, i8*, i32 immarg, i32) nounwind
declare void @llvm.xtensa.mula.da.hh.ldinc(i32 immarg, i8*, i32 immarg, i32) nounwind
declare void @llvm.xtensa.mula.dd.ll.ldinc(i32 immarg, i8*, i32 immarg, i32 immarg) nounwind
declare void @llvm.xtensa.mula.dd.lh.ldinc(i32 immarg, i8*, i32 immarg, i32 immarg) nounwind
declare void @llvm.xtensa.mula.dd.hl.ldinc(i32 immarg, i8*, i32 immarg, i32 immarg) nounwind
declare void @llvm.xtensa.mula.dd.hh.ldinc(i32 immarg, i8*, i32 immarg, i32 immarg) nounwind
declare void @llvm.xtensa.lddec(i32 immarg, i8*) nounwind
declare void @llvm.xtensa.ldinc(i32 immarg, i8*) nounwind
declare i32 @llvm.xtensa.rsr.acclo() nounwind
declare i32 @llvm.xtensa.rsr.acchi() nounwind
declare i32 @llvm.xtensa.rsr.m0() nounwind
declare i32 @llvm.xtensa.rsr.m1() nounwind
declare i32 @llvm.xtensa.rsr.m2() nounwind
declare i32 @llvm.xtensa.rsr.m3() nounwind
declare void @llvm.xtensa.xsr.acclo(i8*) nounwind
declare void @llvm.xtensa.xsr.acchi(i8*) nounwind
declare void @llvm.xtensa.xsr.m0(i8*) nounwind
declare void @llvm.xtensa.xsr.m1(i8*) nounwind
declare void @llvm.xtensa.xsr.m2(i8*) nounwind
declare void @llvm.xtensa.xsr.m3(i8*) nounwind
declare void @llvm.xtensa.wsr.acclo(i32) nounwind
declare void @llvm.xtensa.wsr.acchi(i32) nounwind
declare void @llvm.xtensa.wsr.m0(i32) nounwind
declare void @llvm.xtensa.wsr.m1(i32) nounwind
declare void @llvm.xtensa.wsr.m2(i32) nounwind
declare void @llvm.xtensa.wsr.m3(i32) nounwind
