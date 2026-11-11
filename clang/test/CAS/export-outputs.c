// Check outputs that are written to disk by exporting them from the CAS.

// UNSUPPORTED: system-windows

// RUN: rm -rf %t && mkdir -p %t

// The object file is large enough for the on-disk CAS to store it in its own
// file, which can be cloned when it's exported.
// RUN: %clang -cc1depscan -fdepscan=inline -o %t/t.rsp -cc1-args \
// RUN:   -cc1 -triple x86_64-apple-macos11 -fcas-path %t/cas \
// RUN:   -emit-obj %s -o %t/out.o
// RUN: %clang @%t/t.rsp -fcache-compile-job -Rcompile-job-cache 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CACHE-MISS
// RUN: cp %t/out.o %t/miss.o

// Make the files in the CAS old. A replayed output must look newly written
// even if it's cloned from one of them.
// RUN: %python -c "import os, sys; [os.utime(os.path.join(d, f), (946684800, 946684800)) for d, _, fs in os.walk(sys.argv[1]) for f in fs]" %t/cas
// RUN: rm %t/out.o
// RUN: %clang @%t/t.rsp -fcache-compile-job -Rcompile-job-cache 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CACHE-HIT
// RUN: diff %t/miss.o %t/out.o
// RUN: %python -c "import os, sys; sys.exit(os.stat(sys.argv[1]).st_mtime < 1e9)" %t/out.o

// A file that's not a regular file is written to instead of being replaced,
// both when computing the result and when replaying it.
// RUN: %clang -cc1depscan -fdepscan=inline -o %t/null.rsp -cc1-args \
// RUN:   -cc1 -triple x86_64-apple-macos11 -fcas-path %t/cas-null \
// RUN:   -emit-obj %s -o /dev/null
// RUN: %clang @%t/null.rsp -fcache-compile-job -Rcompile-job-cache 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CACHE-MISS
// RUN: %clang @%t/null.rsp -fcache-compile-job -Rcompile-job-cache 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CACHE-HIT

// CACHE-MISS: remark: compile job cache miss
// CACHE-MISS-NOT: error:
// CACHE-HIT: remark: compile job cache hit
// CACHE-HIT-NOT: error:

const char Data[100 * 1024] = {1};
const char *getData(void) { return Data; }
