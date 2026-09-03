//===-- asan_mac_crashreport.cpp ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// TO_UPSTREAM(crashreporter_global)
//
// Darwin-only adapter that translates the current ASan error into the
// llvm_sanitizer_report_global crash-reporter payload using the public
// asan_interface.h accessors.
//
// We collect every address the public interface can hand out for the
// current report (the access address plus the dealloc/src/dest/first/second
// accessors), dedupe, classify each via __asan_locate_address, and for
// heap-resident addresses pull the allocation / deallocation stacks via
// __asan_get_alloc_stack / __asan_get_free_stack.
//
// Failing stacks (the offending op's call stack — second_free, mismatched
// delete, the offending memory access, etc.) are intentionally not captured
// here since the crash stack (i.e. to the abort in the ASAN runtime) already
// captures that.
//
//===----------------------------------------------------------------------===//

#include "sanitizer_common/sanitizer_platform.h"

#if SANITIZER_APPLE

#  include <stdint.h>

#  include "sanitizer/asan_interface.h"
#  include "sanitizer_common/sanitizer_common.h"
#  include "sanitizer_common/sanitizer_darwin_crashreport.h"
#  include "sanitizer_common/sanitizer_flags.h"

namespace __asan {

namespace {

constexpr unsigned long kTraceCap = LLVM_SANITIZER_V1_FRAMES_PER_STACK;

using StackVec = InternalMmapVector<llvm_sanitizer_report_payload_stack_v1>;

// Append an alloc and (if present) free stack for a single heap address.
void AppendStacksForHeapAddr(StackVec &stacks, uptr addr) {
  void *region_addr = nullptr;
  uptr region_size = 0;
  const char *kind = __asan_locate_address(
      reinterpret_cast<void *>(addr), /*name=*/nullptr, /*name_size=*/0,
      &region_addr, &region_size);
  if (!kind || internal_strcmp(kind, "heap") != 0)
    return;

  void *trace[kTraceCap];
  int thread_id = 0;

  for (unsigned long i = 0; i < kTraceCap; i++) trace[i] = nullptr;
  if (__asan_get_alloc_stack(reinterpret_cast<void *>(addr), trace, kTraceCap,
                             &thread_id))
    GetDarwinStack(stacks, thread_id, trace, kTraceCap,
                   LLVM_SANITIZER_V1_STACK_TYPE_ALLOCATION, "Allocated");

  for (unsigned long i = 0; i < kTraceCap; i++) trace[i] = nullptr;
  thread_id = 0;
  if (__asan_get_free_stack(reinterpret_cast<void *>(addr), trace, kTraceCap,
                            &thread_id))
    GetDarwinStack(stacks, thread_id, trace, kTraceCap,
                   LLVM_SANITIZER_V1_STACK_TYPE_DEALLOCATION, "Freed");
}

void TryAppend(StackVec &stacks, InternalMmapVector<uptr> &seen, uptr addr) {
  if (!addr) return;
  for (uptr i = 0; i < seen.size(); i++)
    if (seen[i] == addr) return;
  seen.push_back(addr);
  AppendStacksForHeapAddr(stacks, addr);
}

}  // namespace

void OnAsanReportPrinted(void) {
  if (!common_flags()->crashreporter_global)
    return;
  if (!__asan_report_present())
    return;

  const char *type_str = __asan_get_report_description();
  if (!type_str || !*type_str)
    type_str = "<unknown>";

  uptr fault_address =
      reinterpret_cast<uptr>(__asan_get_report_address());
  uptr allocation_address = 0, allocation_size = 0;

  // For heap-resident faults, fill in chunk addr/size from locate_address.
  if (fault_address) {
    void *region_addr = nullptr;
    uptr region_size = 0;
    const char *kind = __asan_locate_address(
        reinterpret_cast<void *>(fault_address), nullptr, 0, &region_addr,
        &region_size);
    if (kind && internal_strcmp(kind, "heap") == 0) {
      allocation_address = reinterpret_cast<uptr>(region_addr);
      allocation_size = region_size;
    }
  }

  StackVec stacks;
  stacks.reserve(LLVM_SANITIZER_V1_MAXSTACKS);

  // Walk every address the public accessors can hand out for this report.
  // Each is a candidate for an associated heap allocation whose alloc/free
  // stacks belong in the payload. Different error kinds populate different
  // accessors; we just try them all and dedupe.
  InternalMmapVector<uptr> seen;
  seen.reserve(8);
  const void *out_addr = nullptr;
  uptr out_size = 0;
  TryAppend(stacks, seen, fault_address);
  if (__asan_get_report_dealloc_address(&out_addr, &out_size))
    TryAppend(stacks, seen, reinterpret_cast<uptr>(out_addr));
  if (__asan_get_report_src_address(&out_addr, &out_size))
    TryAppend(stacks, seen, reinterpret_cast<uptr>(out_addr));
  if (__asan_get_report_dest_address(&out_addr, &out_size))
    TryAppend(stacks, seen, reinterpret_cast<uptr>(out_addr));
  if (__asan_get_report_first_address(&out_addr, &out_size))
    TryAppend(stacks, seen, reinterpret_cast<uptr>(out_addr));
  if (__asan_get_report_second_address(&out_addr, &out_size))
    TryAppend(stacks, seen, reinterpret_cast<uptr>(out_addr));

  SetCrashReporterGlobalForReport(type_str, fault_address, allocation_address,
                                  allocation_size, stacks);
}

}  // namespace __asan

#endif  // SANITIZER_APPLE
