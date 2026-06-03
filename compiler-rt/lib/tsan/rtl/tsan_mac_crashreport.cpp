//===-- tsan_mac_crashreport.cpp ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// TO_UPSTREAM(crashreporter_global)
//
// Darwin-only adapter that translates the current TSan report into the
// llvm_sanitizer_report_global crash-reporter payload using the public
// __tsan_get_current_report / __tsan_get_report_* / __tsan_describe_*
// interface.
//
//===----------------------------------------------------------------------===//

#include "sanitizer_common/sanitizer_platform.h"

#if SANITIZER_APPLE && !SANITIZER_GO

#  include <stdint.h>
#  include <string.h>

#  include "sanitizer/tsan_interface.h"
#  include "sanitizer_common/sanitizer_common.h"
#  include "sanitizer_common/sanitizer_darwin_crashreport.h"
#  include "sanitizer_common/sanitizer_flags.h"

namespace __tsan {

namespace {

// Match the payload's per-stack frame capacity so the trace buffer the
// public TSan getters fill never gets truncated before reaching the
// payload.
constexpr unsigned long kTraceCap = LLVM_SANITIZER_V1_FRAMES_PER_STACK;
constexpr unsigned long kDescCap = LLVM_SANITIZER_V1_STACK_DESCRIPTION_MAXLEN;

using StackVec = InternalMmapVector<llvm_sanitizer_report_payload_stack_v1>;

}  // namespace

void OnTsanReportPrinted(void) {
  if (!common_flags()->crashreporter_global)
    return;

  void *report = __tsan_get_current_report();
  if (!report)
    return;

  // Single reusable trace buffer kept off the (small) signal stack.
  InternalMmapVector<void *> trace_buf(kTraceCap);
  void **trace = trace_buf.data();
  auto reset_trace = [&]() {
    for (unsigned long i = 0; i < kTraceCap; i++) trace[i] = nullptr;
  };

  reset_trace();
  const char *type_str = nullptr;
  int count = 0, stack_count = 0, mop_count = 0, loc_count = 0,
      mutex_count = 0, thread_count = 0, unique_tid_count = 0;
  __tsan_get_report_data(report, &type_str, &count, &stack_count, &mop_count,
                         &loc_count, &mutex_count, &thread_count,
                         &unique_tid_count, trace, kTraceCap);

  StackVec stacks;
  stacks.reserve(LLVM_SANITIZER_V1_MAXSTACKS);

  uptr fault_address = 0, allocation_addr = 0, allocation_size = 0;
  char desc[kDescCap];

  for (int i = 0; i < mop_count; i++) {
    int tid = 0, size = 0, write = 0, atomic = 0;
    void *addr = nullptr;
    reset_trace();
    if (!__tsan_get_report_mop(report, i, &tid, &addr, &size, &write, &atomic,
                               trace, kTraceCap))
      continue;
    __tsan_describe_mop(report, i, /*first=*/i == 0, desc, sizeof(desc));
    GetDarwinStack(stacks, tid, trace, kTraceCap,
                   LLVM_SANITIZER_V1_STACK_TYPE_OTHER, desc,
                   /*include_thread_name=*/false);
    if (i == 0)
      fault_address = (uptr)addr;
  }

  for (int i = 0; i < loc_count; i++) {
    const char *loc_type = nullptr;
    void *loc_addr = nullptr;
    void *start = nullptr;
    unsigned long size = 0;
    int tid = 0, fd = 0, suppressable = 0;
    reset_trace();
    if (!__tsan_get_report_loc(report, i, &loc_type, &loc_addr, &start, &size,
                               &tid, &fd, &suppressable, trace, kTraceCap))
      continue;
    int has_stack = __tsan_describe_loc(report, i, desc, sizeof(desc));
    if (has_stack && trace[0])
      GetDarwinStack(stacks, tid, trace, kTraceCap,
                     LLVM_SANITIZER_V1_STACK_TYPE_OTHER, desc,
                     /*include_thread_name=*/false);
    if (loc_type && internal_strcmp(loc_type, "heap") == 0) {
      allocation_addr = reinterpret_cast<uptr>(start);
      allocation_size = size;
    }
  }

  for (int i = 0; i < mutex_count; i++) {
    uint64_t mutex_id = 0;
    void *mutex_addr = nullptr;
    int destroyed = 0;
    reset_trace();
    if (!__tsan_get_report_mutex(report, i, &mutex_id, &mutex_addr, &destroyed,
                                 trace, kTraceCap))
      continue;
    if (!trace[0])
      continue;
    __tsan_describe_mutex(report, i, desc, sizeof(desc));
    GetDarwinStack(stacks, kInvalidTid, trace, kTraceCap,
                   LLVM_SANITIZER_V1_STACK_TYPE_OTHER, desc,
                   /*include_thread_name=*/false);
  }

  for (int i = 0; i < thread_count; i++) {
    int tid = 0, running = 0, parent_tid = 0;
    uint64_t os_id = 0;
    const char *name = nullptr;
    reset_trace();
    if (!__tsan_get_report_thread(report, i, &tid, &os_id, &running, &name,
                                  &parent_tid, trace, kTraceCap))
      continue;
    int has_stack = __tsan_describe_thread(report, i, desc, sizeof(desc));
    if (has_stack && trace[0])
      GetDarwinStack(stacks, parent_tid, trace, kTraceCap,
                     LLVM_SANITIZER_V1_STACK_TYPE_OTHER, desc,
                     /*include_thread_name=*/false);
  }

  SetCrashReporterGlobalForReport(type_str ? type_str : "<unknown>",
                                  fault_address, allocation_addr,
                                  allocation_size, stacks);
}

}  // namespace __tsan

#endif  // SANITIZER_APPLE && !SANITIZER_GO
