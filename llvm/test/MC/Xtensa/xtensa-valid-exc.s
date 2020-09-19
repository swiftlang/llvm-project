# RUN: llvm-mc %s -triple=xtensa -mattr=+exception -show-encoding \
# RUN:     | FileCheck -check-prefixes=CHECK,CHECK-INST %s

.align	4
LBL0:

# CHECK-INST: excw
# CHECK: encoding: [0x80,0x20,0x00]
 excw

# CHECK-INST: rfde
# CHECK: encoding: [0x00,0x32,0x00]
 rfde

# CHECK-INST: rfe
# CHECK: encoding: [0x00,0x30,0x00]
 rfe

# CHECK-INST: syscall
# CHECK: encoding: [0x00,0x50,0x00]
 syscall
