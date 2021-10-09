target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx12.0.0"

define i32 @_foo() #0 {
  ret i32 0
}

define i32 @main() #0 {
  %1 = call i32 @_foo()
  ret i32 %1
}

attributes #0 = { noinline nounwind optnone }
