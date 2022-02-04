//===- DiagnosticCASKinds.td - CAS diagnostics ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

let Component = "CAS" in {

def err_cas_plugin_cannot_be_loaded : Error<
  "CAS plugin '%0' cannot be loaded (check -fcas-plugin-path and"
  " -fcas-plugin-args)">, DefaultFatal;
def err_builtin_cas_cannot_be_initialized : Error<
  "Builtin CAS cannot be initialized from '%0' on disk"
  " (check -fcas-builtin-path)">, DefaultFatal;
def err_cas_builtin_path : Error<
  "Builtin CAS cannot be initialized from '%0' on disk"
  " (check -fcas-builtin-path)">, DefaultFatal;
def err_cas_cannot_parse_root_id : Error<
  "CAS cannot parse root-id '%0' specified by -fcas-fs">, DefaultFatal;
def err_cas_filesystem_cannot_be_initialized : Error<
  "CAS filesystem cannot be initialized from root-id '%0' specified by"
  " -fcas-fs">, DefaultFatal;
def err_cas_filesystem_cannot_set_working_directory : Error<
  "CAS filesystem cannot set working directory to '%0' specified by"
  " -fcas-fs-working-directory">, DefaultFatal;
def err_cas_depscan_daemon_connection: Error<
  "Failed to establish connection with depscan daemon: %0">, DefaultFatal;

def remark_cas_fs_result_cache_hit : Remark<
  "result cache hit for '%0' => '%1'">, InGroup<CASFSResultCacheHit>;
def remark_cas_fs_result_cache_miss : Remark<
  "result cache miss for '%0'">, InGroup<CASFSResultCacheMiss>;

} // let Component = "CAS" in
