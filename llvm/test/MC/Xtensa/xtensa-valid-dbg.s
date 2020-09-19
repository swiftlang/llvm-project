# RUN: llvm-mc %s -triple=xtensa -mattr=+debug -show-encoding \
# RUN:     | FileCheck -check-prefixes=CHECK,CHECK-INST %s

.align	4
LBL0:

# CHECK-INST:  break     1, 2
# CHECK: encoding: [0x20,0x41,0x00]
 break 1, 2
