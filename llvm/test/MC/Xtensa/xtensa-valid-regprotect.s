# RUN: llvm-mc %s -triple=xtensa -mattr=+regprotect -show-encoding \
# RUN:     | FileCheck -check-prefixes=CHECK,CHECK-INST %s


.align	4
LBL0:

# CHECK-INST:  wdtlb	a3, a4
# CHECK: encoding: [0x30,0xe4,0x50]
 wdtlb a3, a4

# CHECK-INST:  witlb	a3, a4
# CHECK: encoding: [0x30,0x64,0x50]
 witlb a3, a4
