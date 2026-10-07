//===-- sanitizer_darwin_crashreport.cpp ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// TO_UPSTREAM(crashreporter_global)

#include "sanitizer_platform.h"

#if SANITIZER_APPLE

#  include "sanitizer_common.h"
#  include "sanitizer_darwin_crashreport.h"
#  include "sanitizer_libc.h"

llvm_sanitizer_report_payload llvm_sanitizer_report_global;

namespace __sanitizer {

void GetDarwinStack(
    InternalMmapVector<llvm_sanitizer_report_payload_stack_v1> &stacks, int tid,
    void *const *trace, uptr cap, uint16_t type, const char *description,
    bool include_thread_name) {
  stacks.resize(stacks.size() + 1);
  llvm_sanitizer_report_payload_stack_v1 &res = stacks.back();
  internal_memset(&res, 0, sizeof(res));
  res.type = type;
  res.stack.thread_id = tid;
  res.stack.time = 0;

  // Count populated leading slots in `trace` (the public-API getters fill
  // in order and leave the rest zeroed) and clamp to the payload capacity.
  uptr n = 0;
  while (n < cap && trace[n]) ++n;
  const uptr payload_cap =
      sizeof(res.stack.frames) / sizeof(res.stack.frames[0]);
  res.stack.num_frames = Min(n, payload_cap);

  // Public-API traces are top-of-stack-first; the payload stores them
  // top-of-stack-last.
  for (uptr i = 0; i < res.stack.num_frames; i++) {
    res.stack.frames[i] =
        (uintptr_t)trace[res.stack.num_frames - i - 1];
  }
  if (include_thread_name) {
    internal_snprintf(res.description, sizeof(res.description),
                      "%s by thread %d", description, tid);
  } else {
    internal_strncpy(res.description, description, sizeof(res.description));
    res.description[sizeof(res.description) - 1] = 0;
  }
}

void SetCrashReporterGlobalForReport(
    const char *error_name, uptr fault_addr, uptr allocation_addr,
    uptr allocation_size,
    const InternalMmapVector<llvm_sanitizer_report_payload_stack_v1> &stacks) {
  llvm_sanitizer_report_global.vers = 1;
  internal_strncpy(llvm_sanitizer_report_global.v1.category, SanitizerToolName,
                   LLVM_SANITIZER_V1_CATEGORY_MAXLEN);
  llvm_sanitizer_report_global.v1
      .category[LLVM_SANITIZER_V1_CATEGORY_MAXLEN - 1] = 0;
  internal_strncpy(llvm_sanitizer_report_global.v1.type, error_name,
                   LLVM_SANITIZER_V1_TYPE_MAXLEN);
  llvm_sanitizer_report_global.v1.type[LLVM_SANITIZER_V1_TYPE_MAXLEN - 1] = 0;

  llvm_sanitizer_report_global.v1.fault_address = fault_addr;
  llvm_sanitizer_report_global.v1.allocation_address = allocation_addr;
  llvm_sanitizer_report_global.v1.allocation_size = allocation_size;

  llvm_sanitizer_report_global.v1.nstacks =
      Min(stacks.size(), (uptr)LLVM_SANITIZER_V1_MAXSTACKS);
  internal_memcpy(llvm_sanitizer_report_global.v1.stacks, stacks.data(),
                  llvm_sanitizer_report_global.v1.nstacks *
                      sizeof(llvm_sanitizer_report_payload_stack_v1));
}

}  // namespace __sanitizer

#endif  // SANITIZER_APPLE
