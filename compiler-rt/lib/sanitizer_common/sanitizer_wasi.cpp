//===-- sanitizer_wasi.cpp ------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file is shared between AddressSanitizer and ThreadSanitizer
// run-time libraries and implements WASI-specific functions from
// sanitizer_posix.h.
//===----------------------------------------------------------------------===//

#include "sanitizer_platform.h"

#if SANITIZER_WASI

#  include <limits.h>
#  include <stdlib.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <time.h>
#  include <errno.h>
#  include <wasi/api.h>

#  include "sanitizer_common.h"
#  include "sanitizer_libc.h"
#  include "sanitizer_file.h"
#  include "sanitizer_symbolizer.h"
#  include "sanitizer_atomic.h"
#  include "sanitizer_mutex.h"
#  include "sanitizer_stacktrace.h"
#  include "sanitizer_symbolizer_internal.h"

namespace __sanitizer {

extern "C" void *__libc_malloc(uptr);
extern "C" void __libc_free(void *);

void InitializePlatformEarly() {}
void InitTlsSize() {}

const char *GetEnv(const char *name) { return nullptr; }

uptr GetPageSize() { return PAGESIZE; }

void *MmapOrDie(uptr size, const char *mem_type, bool raw_report) {
  size = RoundUpTo(size, GetPageSize());
  void *ptr = __libc_malloc(size);
  if (!ptr) {
    if (raw_report) {
      Report("MmapOrDie: failed to allocate %zu bytes\n", size);
    }
    Die();
  }
  return ptr;
}

void UnmapOrDie(void *addr, uptr size, bool raw_report) {
  __libc_free(addr);
}

void *MmapNoReserveOrDie(uptr size, const char *mem_type) {
  return MmapOrDie(size, mem_type, false);
}

void DumpProcessMap() {
    return;
}

// Implement mandatory functions for ASan WASI build
void CheckASLR() {}
void PlatformPrepareForSandboxing(void *args) {}
void DisableCoreDumperIfNecessary() {}
void InstallDeadlySignalHandlers(void (*cb)(int, void *, void *)) {}
int Atexit(void (*function)(void)) { return 0; }
uptr GetMaxUserVirtualAddress() { return (1ULL << 30); } // 1GB for WASI
uptr GetMmapGranularity() { return GetPageSize(); }
uptr internal_sched_yield() { return 0; }
void internal_join_thread(void *th) {}
bool MemoryRangeIsAvailable(uptr beg, uptr size) { return true; }
void GetThreadStackAndTls(bool main, uptr *stk_addr, uptr *stk_size,
                           uptr *tls_addr, uptr *tls_size) {
  *stk_addr = 0;
  *stk_size = 0;
  *tls_addr = 0;
  *tls_size = 0;
}

uptr internal_getpid() { return 1; }
void SetAlternateSignalStack() {}
uptr GetThreadSelf() { return 0; }

void Symbolizer::LateInitialize() {
  Symbolizer::GetOrInit();
}

// Additional mandatory functions
void *MmapOrDieOnFatalError(uptr size, const char *mem_type) {
  size = RoundUpTo(size, GetPageSizeCached()) + GetPageSizeCached();
  void *ptr = __libc_malloc(size);
  if (!ptr) {
    Report("MmapOrDieOnFatalError: failed to allocate %zu bytes\n", size);
    Die();
  }
  IncreaseTotalMmap(size);
  ptr = (void *)RoundUpTo((uptr)ptr, GetPageSizeCached());
  return (void *)ptr;
}
void *MmapAlignedOrDieOnFatalError(uptr size, uptr alignment, const char *mem_type) { 
  CHECK(IsPowerOfTwo(size));
  CHECK(IsPowerOfTwo(alignment));
  uptr map_size = size + alignment;
  // mmap maps entire pages and rounds up map_size needs to be a an integral
  // number of pages.
  // We need to be aware of this size for calculating end and for unmapping
  // fragments before and after the alignment region.
  map_size = RoundUpTo(map_size, GetPageSizeCached());
  uptr map_res = (uptr)MmapOrDieOnFatalError(map_size, mem_type);
  if (UNLIKELY(!map_res))
    return nullptr;
  uptr res = map_res;
  if (!IsAligned(res, alignment)) {
    res = (map_res + alignment - 1) & ~(alignment - 1);
  }
  return (void*)res;
}

uptr ReadLongProcessName(char *buf, uptr buf_len) { 
  internal_strncpy(buf, "wasi-process", buf_len);
  return internal_strlen("wasi-process");
}

uptr ReadBinaryName(char *buf, uptr buf_len) {
  internal_strncpy(buf, "wasi-binary", buf_len);
  return internal_strlen("wasi-binary");
}

char **GetArgv() { return nullptr; }

void internal_usleep(u64 useconds) { }

bool WriteToFile(int fd, const void *buf, uptr count, uptr *written, int *errno_p) {
  __wasi_ciovec_t iov;
  iov.buf = static_cast<const uint8_t*>(buf);
  iov.buf_len = count;
  
  __wasi_size_t wasi_written;
  __wasi_errno_t error = __wasi_fd_write(fd, &iov, 1, &wasi_written);
  
  if (written)
    *written = wasi_written;
  if (errno_p)
    *errno_p = error;
  
  return error == 0;
}

void CloseFile(fd_t fd) { 
  __wasi_errno_t error = __wasi_fd_close(fd);
  (void)error;
}

int OpenFile(const char *filename, FileAccessMode mode, int *errno_p) {
  if (errno_p) *errno_p = 0;
  return -1;
}

bool IsPathSeparator(char c) {
  return c == '/';
}

bool DirExists(const char *path) {
  return false;
}

// More mandatory functions
bool ReadFromFile(int fd, void *buff, uptr buff_size, uptr *bytes_read, int *error_p) {
  __wasi_iovec_t iov;
  iov.buf = static_cast<uint8_t*>(buff);
  iov.buf_len = buff_size;
  
  __wasi_size_t wasi_bytes_read;
  __wasi_errno_t error = __wasi_fd_read(fd, &iov, 1, &wasi_bytes_read);
  
  if (bytes_read)
    *bytes_read = wasi_bytes_read;
  if (error_p)
    *error_p = error;
  
  return error == 0;
}

bool CreateDir(const char *path) {
  return false;
}

void InitializePlatformCommonFlags(CommonFlags *cf) {}

void FutexWait(atomic_uint32_t *p, u32 cmp) {}

void FutexWake(atomic_uint32_t *p, u32 count) {}

bool FileExists(const char *filename) {
  return false;
}

bool IsAbsolutePath(const char *path) {
  return path && path[0] == '/';
}

void Abort() {
  __builtin_unreachable();
}

void NORETURN internal__exit(int exitcode) {
  __wasi_proc_exit(exitcode);
}

tid_t GetTid() {
  return 0;
}

bool MmapFixedSuperNoReserve(uptr fixed_addr, uptr size, const char *name) {
  __builtin_unreachable();
  return false;
}

bool DontDumpShadowMemory(uptr addr, uptr length) {
  return false;
}

bool MmapFixedNoReserve(uptr fixed_addr, uptr size, const char *name) {
  __builtin_unreachable();
  return false;
}

void *MmapFixedNoAccess(uptr fixed_addr, uptr size, const char *name) {
  __builtin_unreachable();
  return nullptr;
}

bool MprotectReadOnly(uptr addr, uptr size) {
  return false;
}

void *internal_start_thread(void *(*func)(void *arg), void *arg) {
  return nullptr;
}

u64 MonotonicNanoTime() {
  return 0;
}

// Additional functions needed for symbolizer

// These functions need to be properly defined
#if SANITIZER_WASI
void BufferedStackTrace::UnwindSlow(uptr pc, u32 max_depth) {
  size = 0;
}

void BufferedStackTrace::UnwindSlow(uptr pc, void *context, u32 max_depth) {
  size = 0;
}


class WASISymbolizerTool : public SymbolizerTool {
 public:
  bool SymbolizePC(uptr addr, SymbolizedStack *stack) override;
  bool SymbolizeData(uptr addr, DataInfo *info) override {
    return false;
  }
  const char *Demangle(const char *name) override {
    return name;
  }
};

bool WASISymbolizerTool::SymbolizePC(uptr addr, SymbolizedStack *frame) {
  return false;
}

static void ChooseSymbolizerTools(IntrusiveList<SymbolizerTool> *list,
                                  LowLevelAllocator *allocator) {
  if (!common_flags()->symbolize) {
    VReport(2, "Symbolizer is disabled.\n");
    return;
  }

  list->push_back(new(*allocator) WASISymbolizerTool());
}


Symbolizer *Symbolizer::PlatformInit() {
  IntrusiveList<SymbolizerTool> list;
  list.clear();
  ChooseSymbolizerTools(&list, &symbolizer_allocator_);

  return new(symbolizer_allocator_) Symbolizer(list);
}

void ListOfModules::init() {
  modules_.Initialize(2);

  char name[] = "wasi-binary";

  LoadedModule main_module;
  main_module.set(name, 0);

  // Emscripten represents program counters as offsets into WebAssembly
  // modules. For JavaScript code, the "program counter" is the line number
  // of the JavaScript code with the high bit set.
  // Therefore, PC values 0x80000000 and beyond represents JavaScript code.
  // As a result, 0x00000000 to 0x7FFFFFFF represents PC values for WASM code.
  // We consider WASM code as main_module.
  main_module.addAddressRange(0, 0x7FFFFFFF, /*executable*/ true,
                              /*writable*/ false);
  modules_.push_back(main_module);
}

void ListOfModules::fallbackInit() {}

const char *Symbolizer::PlatformDemangle(const char *name) {
  return name;
}

bool SupportsColoredOutput(fd_t fd) {
  __wasi_fdstat_t statbuf;
  int err = __wasi_fd_fdstat_get(fd, &statbuf);
  if (err != 0) {
    errno = err;
    return 0;
  }
  return statbuf.fs_filetype == __WASI_FILETYPE_CHARACTER_DEVICE;
}

bool SignalContext::IsStackOverflow() const {
  return false;
}

const char *SignalContext::Describe() const {
  return "WASI signal";
}

bool IsAccessibleMemoryRange(uptr addr, uptr size) {
  return false;
}

void SignalContext::DumpAllRegisters(void *context) {}
#endif // SANITIZER_WASI

}  // namespace __sanitizer

namespace __sanitizer {
// ReportFile implementation for WASI
void ReportFile::Write(const char *buffer, uptr length) {
  // Use stderr for error reporting (fd 2 in WASI)
  __wasi_ciovec_t iov;
  iov.buf = (const uint8_t*)buffer;
  iov.buf_len = length;
  
  __wasi_size_t written;
  __wasi_errno_t error = __wasi_fd_write(2, &iov, 1, &written);
  (void)error;
}
}

#endif
