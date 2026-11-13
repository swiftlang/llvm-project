;; Check that ThinLTO cache requests through a CAS plugin that complete after
;; ThinLTO stopped waiting for them don't use the state that was destroyed by
;; then, which AddressSanitizer reports, and that their callbacks don't release
;; the last reference to the CAS, because the test plugin can't be disposed of
;; from its own threads. The callbacks run on another thread, so the process can
;; exit before a report is complete; check that none starts. The callbacks also
;; must not log after ThinLTO stopped waiting.

; REQUIRES: thread_support, x86-registered-target

; RUN: rm -rf %t && split-file %s %t
; RUN: opt -module-hash -module-summary %t/a.ll -o %t/a.bc
; RUN: opt -module-hash -module-summary %t/b.ll -o %t/b.bc

; DEFINE: %{plugin} = %llvmshlibdir/CASPluginTest%pluginext
; DEFINE: %{populate} = llvm-lto -thinlto-action=run -exported-symbol=a -exported-symbol=b
; DEFINE: %{lto} = %{populate} -thinlto-cache-logging -thinlto-wait-after-run-ms=1000
; DEFINE: %{filecheck} = FileCheck %s --implicit-check-not=AddressSanitizer

;; The cache lookups hit, but loading the objects from upstream completes after
;; ThinLTO stopped waiting.
; RUN: %{populate} %t/a.bc %t/b.bc \
; RUN:   -thinlto-cache-dir plugin:%{plugin}:%t/cas1?upstream-path=%t/upstream1:no-logging=1
; RUN: %{lto} %t/a.bc %t/b.bc -thinlto-cache-timeout-ms=0 \
; RUN:   -thinlto-cache-dir plugin:%{plugin}:%t/cas2?upstream-path=%t/upstream1:async-load-delay-ms=200 \
; RUN:   2>&1 | %{filecheck} --check-prefix=LOAD

; LOAD:     Handled outstanding cache requests
; LOAD-NOT: LTO cache
; LOAD:     load_object_async downstream end
; LOAD-NOT: LTO cache

;; Nothing is cached, and updating the cache completes after ThinLTO stopped
;; waiting.
; RUN: %{lto} %t/a.bc %t/b.bc -thinlto-cache-timeout-ms=200 \
; RUN:   -thinlto-cache-dir plugin:%{plugin}:%t/cas3?upstream-path=%t/upstream3:no-logging=1:async-put-delay-ms=600 \
; RUN:   2>&1 | %{filecheck} --check-prefix=PUT

; PUT-NOT:  LTO cache update
; PUT:      Handled outstanding cache requests
; PUT-NOT:  LTO cache

;; Only a.bc is cached. Its delayed cache hit is the last cache read to
;; complete, and its callback stores the object of b.bc to update the cache,
;; which completes after ThinLTO stopped waiting for the cache update.
; RUN: %{populate} %t/a.bc \
; RUN:   -thinlto-cache-dir plugin:%{plugin}:%t/cas4?upstream-path=%t/upstream4:no-logging=1
; RUN: %{lto} %t/a.bc %t/b.bc -thinlto-cache-timeout-ms=400 \
; RUN:   -thinlto-cache-dir plugin:%{plugin}:%t/cas5?upstream-path=%t/upstream4:no-logging=1:async-load-delay-ms=200:callback-store-delay-ms=800 \
; RUN:   2>&1 | %{filecheck} --check-prefix=STORE

; STORE:     LTO cache load
; STORE:     Handled outstanding cache requests
; STORE-NOT: LTO cache

;--- a.ll
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx10.11.0"

define void @a() {
  ret void
}

;--- b.ll
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx10.11.0"

define void @b() {
  ret void
}
