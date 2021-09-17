//===- PluginCAS.cpp --------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm-c/CASPlugin.h"
#include "llvm/ADT/Optional.h"
#include "llvm/CAS/CASDB.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/Error.h"

#ifndef _WIN32
#include <dlfcn.h>
#endif

using namespace llvm;
using namespace llvm::cas;

namespace {
class PluginCAS : public CASDB {
public:
  Expected<CASID> parseCASID(StringRef Reference) final {
    return createStringError(llvm::inconvertibleErrorCode(), "not implemented");
  }
  Error printCASID(raw_ostream &OS, CASID ID) final {
    return createStringError(llvm::inconvertibleErrorCode(), "not implemented");
  }
  Expected<BlobRef> createBlob(StringRef Data) final {
    return createStringError(llvm::inconvertibleErrorCode(), "not implemented");
  }
  Expected<TreeRef> createTree(ArrayRef<NamedTreeEntry> Entries) final {
    return createStringError(llvm::inconvertibleErrorCode(), "not implemented");
  }
  Expected<NodeRef> createNode(ArrayRef<CASID> References,
                               StringRef Data) final {
    return createStringError(llvm::inconvertibleErrorCode(), "not implemented");
  }
  Expected<BlobRef> getBlob(CASID ID) final {
    return createStringError(llvm::inconvertibleErrorCode(), "not implemented");
  }
  Expected<TreeRef> getTree(CASID ID) final {
    return createStringError(llvm::inconvertibleErrorCode(), "not implemented");
  }
  Expected<NodeRef> getNode(CASID ID) final {
    return createStringError(llvm::inconvertibleErrorCode(), "not implemented");
  }
  Optional<ObjectKind> getObjectKind(CASID ID) final;

  Error putCachedResult(CASID InputID, CASID OutputID) final {
    return createStringError(llvm::inconvertibleErrorCode(), "not implemented");
  }
  Expected<CASID> getCachedResult(CASID InputID) final {
    return createStringError(llvm::inconvertibleErrorCode(), "not implemented");
  }
  Optional<NamedTreeEntry> lookupInTree(const TreeRef &Tree,
                                        StringRef Name) const final {
    llvm_unreachable("not implemented");
  }
  NamedTreeEntry getInTree(const TreeRef &Tree, size_t I) const final {
    llvm_unreachable("not implemented");
  }
  Error forEachEntryInTree(
      const TreeRef &Tree,
      function_ref<Error(const NamedTreeEntry &)> Callback) const final {
    return createStringError(llvm::inconvertibleErrorCode(), "not implemented");
  }

  CASID getReferenceInNode(const NodeRef &Ref, size_t I) const final {
    llvm_unreachable("not implemented");
  }
  Error
  forEachReferenceInNode(const NodeRef &Ref,
                         function_ref<Error(CASID)> Callback) const final {
    return createStringError(llvm::inconvertibleErrorCode(), "not implemented");
  }

  Error initialize();
  explicit PluginCAS(std::string PluginPath, sys::DynamicLibrary DL)
      : PluginPath(std::move(PluginPath)), DL(DL) {}

  ~PluginCAS() {
    if (DB)
      api->close_db(DB);
  }

private:
  llvm_cas_db_handle DB = nullptr;
  std::string PluginPath;
  sys::DynamicLibrary DL;
  Optional<llvm_cas_plugin_api> api;
};
} // namespace

Optional<ObjectKind> PluginCAS::getObjectKind(CASID ID) {
  llvm_cas_id_t RawID{ID.getHash().begin(), ID.getHash().size()};
  switch (api->get_object_kind(DB, &RawID)) {
  default:
    return None;
  case LLVM_CAS_OBJECT_BLOB:
    return ObjectKind::Blob;
  case LLVM_CAS_OBJECT_TREE:
    return ObjectKind::Tree;
  case LLVM_CAS_OBJECT_NODE:
    return ObjectKind::Node;
  }
}

Error PluginCAS::initialize() {
  api.emplace();
  auto Initializer =
      reinterpret_cast<decltype(&llvm_cas_plugin_initialize_api)>(
          DL.getAddressOfSymbol("llvm_cas_plugin_initialize_api"));
  // FIXME: error_code is wrong.
  if (!Initializer(&*api))
    return createStringError(std::error_code(),
                             "initialization failed: " + PluginPath);
  return Error::success();
}

static Expected<sys::DynamicLibrary> openLibrary(const char *PluginPath) {
  using sys::DynamicLibrary;
  std::string Error;
  DynamicLibrary DL = DynamicLibrary::getPermanentLibrary(PluginPath, &Error);
  // FIXME: error_code is wrong.
  if (!Error.empty())
    return createStringError(std::error_code(), Error);
  return DL;
}

Expected<std::unique_ptr<CASDB>>
cas::createPluginCAS(StringRef PluginPath, ArrayRef<std::string> PluginArgs) {
  std::string PluginPathString = PluginPath.str();
  auto ExpectedDL = openLibrary(PluginPathString.c_str());
  if (!ExpectedDL)
    return ExpectedDL.takeError();
  auto Plugin = std::make_unique<PluginCAS>(std::move(PluginPathString),
                                            std::move(*ExpectedDL));
  if (Error E = Plugin->initialize())
    return std::move(E);
  return std::move(Plugin);
}
