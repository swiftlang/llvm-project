//===-- asan_wasi.cpp -----------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file is a part of AddressSanitizer, an address sanity checker.
//
// WASI-specific details.
//===----------------------------------------------------------------------===//

#include "sanitizer_common/sanitizer_platform.h"
#include "sanitizer_common/sanitizer_internal_defs.h"

#if SANITIZER_WASI

#include "asan/asan_poisoning.h"
#include "asan_interceptors.h"
#include "asan_internal.h"

namespace __asan {

void InitializeShadowMemory() {
  // Poison the shadow memory itself to catch invalid shadow accesses and
  // also to catch null pointer dereferences.
  FastPoisonShadow(kLowShadowBeg, kLowShadowEnd - kLowShadowBeg, kAsanGlobalRedzoneMagic);
}

void InitializePlatformInterceptors() {}
void InitializePlatformExceptionHandlers() {}

void AsanCheckDynamicRTPrereqs() {}
void AsanCheckIncompatibleRT() {}
void InitializeAsanInterceptors() {}

void AsanOnDeadlySignal(int signo, void *siginfo, void *context) {
  UNIMPLEMENTED();
}

bool PlatformUnpoisonStacks() { return false; }

// Simple thread local storage implementation for WASI
static thread_local void *per_thread;

void *AsanTSDGet() { return per_thread; }

void AsanTSDSet(void *tsd) { per_thread = tsd; }

void AsanTSDInit(void (*destructor)(void *tsd)) {
  DCHECK(destructor == &PlatformTSDDtor);
}

void PlatformTSDDtor(void *tsd) { UNREACHABLE(__func__); }

void AsanApplyToGlobals(globals_op_fptr op, const void *needle) {
  UNIMPLEMENTED();
}

void InstallAtForkHandler() {
  // WASI doesn't support fork
}

void FlushUnneededASanShadowMemory(uptr p, uptr size) {
  // No-op as madvise is not supported on WASI
}

// On WASI, leak detection is not supported yet
void InstallAtExitCheckLeaks() {}

// On WASI Preview 1, dlopen is not supported
bool HandleDlopenInit() { return false; }

// WASI does not support ASLR
void TryReExecWithoutASLR() {}

}  // namespace __asan

#endif // SANITIZER_WASI
