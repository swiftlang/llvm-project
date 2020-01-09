// RUN: %clang_cc1 -std=c++11 -triple i686-linux-gnu %s -o /dev/null -S -emit-llvm
//
// This test's failure mode is running ~forever. (For some value of "forever"
// that's greater than 25 minutes on my machine)
