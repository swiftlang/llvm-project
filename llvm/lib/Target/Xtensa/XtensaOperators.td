//===- XtensaOperators.td - Xtensa-specific operators ---------*- tblgen-*-===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Type profiles
//===----------------------------------------------------------------------===//

def SDT_XtensaCallSeqStart        : SDCallSeqStart<[SDTCisVT<0, i32>, SDTCisVT<1, i32>]>;
def SDT_XtensaCallSeqEnd          : SDCallSeqEnd<[SDTCisVT<0, i32>, SDTCisVT<1, i32>]>;
def SDT_XtensaCall                : SDTypeProfile<0, -1, [SDTCisPtrTy<0>]>;
def SDT_XtensaWrapPtr             : SDTypeProfile<1, 1,
                                                 [SDTCisSameAs<0, 1>,
                                                  SDTCisPtrTy<0>]>;

def SDT_XtensaSelectCC            : SDTypeProfile<1, 5,
                                                 [SDTCisSameAs<0, 1>,
                                                  SDTCisSameAs<2, 3>,
                                                  SDTCisVT<5, i32>]>;
def SDT_XtensaBrJT                : SDTypeProfile<0, 2,
                                                 [SDTCisPtrTy<0>, SDTCisVT<1, i32>]>;

def SDT_XtensaSHL                 : SDTypeProfile<1, 1, [SDTCisVT<0, i32>, SDTCisVT<1, i32>]>;
def SDT_XtensaSRA                 : SDTypeProfile<1, 1, [SDTCisVT<0, i32>, SDTCisVT<1, i32>]>;
def SDT_XtensaSRL                 : SDTypeProfile<1, 1, [SDTCisVT<0, i32>, SDTCisVT<1, i32>]>;
def SDT_XtensaSRC                 : SDTypeProfile<1, 2, [SDTCisVT<0, i32>, SDTCisVT<1, i32>,
                                                         SDTCisVT<2, i32>]>;
def SDT_XtensaSSL                 : SDTypeProfile<0, 1, [SDTCisVT<0, i32>]>;
def SDT_XtensaSSR                 : SDTypeProfile<0, 1, [SDTCisVT<0, i32>]>;

//===----------------------------------------------------------------------===//
// Node definitions
//===----------------------------------------------------------------------===//

def Xtensa_call: SDNode<"XtensaISD::CALL", SDT_XtensaCall,
                       [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue, SDNPVariadic]>;

def Xtensa_retflag: SDNode<"XtensaISD::RET_FLAG", SDTNone,
                          [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;


def Xtensa_callseq_start: SDNode<"ISD::CALLSEQ_START", SDT_XtensaCallSeqStart,
                                [SDNPHasChain, SDNPSideEffect, SDNPOutGlue]>;
def Xtensa_callseq_end  : SDNode<"ISD::CALLSEQ_END",   SDT_XtensaCallSeqEnd,
                                [SDNPHasChain, SDNPSideEffect, SDNPOptInGlue,
                                 SDNPOutGlue]>;

def Xtensa_pcrel_wrapper: SDNode<"XtensaISD::PCREL_WRAPPER", SDT_XtensaWrapPtr, []>;

def Xtensa_select : SDNode<"XtensaISD::SELECT", SDTSelect>;
def Xtensa_select_cc: SDNode<"XtensaISD::SELECT_CC", SDT_XtensaSelectCC,
                            [SDNPInGlue]>;

def Xtensa_shl: SDNode<"XtensaISD::SHL", SDT_XtensaSHL, [SDNPInGlue]>;
def Xtensa_sra: SDNode<"XtensaISD::SRA", SDT_XtensaSRA, [SDNPInGlue]>;
def Xtensa_srl: SDNode<"XtensaISD::SRL", SDT_XtensaSRL, [SDNPInGlue]>;
def Xtensa_src: SDNode<"XtensaISD::SRC", SDT_XtensaSRC, [SDNPInGlue]>;
def Xtensa_ssl: SDNode<"XtensaISD::SSL", SDT_XtensaSSL, [SDNPOutGlue]>;
def Xtensa_ssr: SDNode<"XtensaISD::SSR", SDT_XtensaSSR, [SDNPOutGlue]>;

def Xtensa_brjt: SDNode<"XtensaISD::BR_JT", SDT_XtensaBrJT, [SDNPHasChain]>;
