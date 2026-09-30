//===-- sanitizer_darwin_crashreport.h --------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// TO_UPSTREAM(crashreporter_global)
//===----------------------------------------------------------------------===//
#ifndef SANITIZER_DARWIN_CRASHREPORT_H
#define SANITIZER_DARWIN_CRASHREPORT_H

#include "sanitizer_common.h"
#include "sanitizer_platform.h"

#if SANITIZER_APPLE

#  include <stddef.h>
#  include <stdint.h>

#  define LLVM_SANITIZER_V1_CATEGORY_MAXLEN 32
#  define LLVM_SANITIZER_V1_TYPE_MAXLEN 32
#  define LLVM_SANITIZER_V1_STACK_DESCRIPTION_MAXLEN 128
#  define LLVM_SANITIZER_V1_MAXSTACKS 4
#  define LLVM_SANITIZER_V1_FRAMES_PER_STACK 64

#  define LLVM_SANITIZER_V1_STACK_TYPE_OTHER 0
#  define LLVM_SANITIZER_V1_STACK_TYPE_ALLOCATION 1
#  define LLVM_SANITIZER_V1_STACK_TYPE_DEALLOCATION 2

typedef struct {
  uint64_t thread_id;
  uint64_t time;
  uint32_t num_frames;
  uintptr_t frames[LLVM_SANITIZER_V1_FRAMES_PER_STACK];
} sanitizers_stack_trace_t;

typedef struct {
  uint32_t type;
  char description[LLVM_SANITIZER_V1_STACK_DESCRIPTION_MAXLEN];
  sanitizers_stack_trace_t stack;
} llvm_sanitizer_report_payload_stack_v1;

typedef struct {
  char category[LLVM_SANITIZER_V1_CATEGORY_MAXLEN];
  char type[LLVM_SANITIZER_V1_TYPE_MAXLEN];

  // These three fields may be 0 for non-heap errors.
  uintptr_t fault_address;
  uintptr_t allocation_address;
  size_t allocation_size;

  uint16_t nstacks;
  llvm_sanitizer_report_payload_stack_v1 stacks[LLVM_SANITIZER_V1_MAXSTACKS];
} llvm_sanitizer_report_payload_v1;

typedef struct __attribute__((packed)) {
  uint16_t vers;
  union {
    llvm_sanitizer_report_payload_v1 v1;
  };
} llvm_sanitizer_report_payload;

namespace __sanitizer {

// Append a single backtrace + description tuple to `stacks` in the Darwin
// crash-reporter payload format. `trace` is a buffer of `cap` PC slots in
// top-of-stack-first order (matching what `__asan_get_alloc_stack` /
// `__tsan_get_report_mop` write); the populated length is taken to be the
// number of non-null leading slots. The payload's frames[] array is written
// in the reversed (top-of-stack-last) order. If `include_thread_name` is
// true, the description is suffixed with "by thread <tid>".
void GetDarwinStack(
    InternalMmapVector<llvm_sanitizer_report_payload_stack_v1> &stacks, int tid,
    void *const *trace, uptr cap, uint16_t type, const char *description,
    bool include_thread_name = true);

// Populate `llvm_sanitizer_report_global` with the given report metadata and
// stacks. Truncates fields that don't fit. Called once per report.
void SetCrashReporterGlobalForReport(
    const char *error_name, uptr fault_addr, uptr allocation_addr,
    uptr allocation_size,
    const InternalMmapVector<llvm_sanitizer_report_payload_stack_v1> &stacks);

}  // namespace __sanitizer

#endif  // SANITIZER_APPLE
#endif  // SANITIZER_DARWIN_CRASHREPORT_H
