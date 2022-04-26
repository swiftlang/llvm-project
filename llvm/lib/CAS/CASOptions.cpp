//===- CASOptions.cpp -----------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/CASDB.h"
#include "llvm/CAS/CASOptions.h"
#include "llvm/Support/YAMLTraits.h"

using namespace llvm;
using namespace llvm::cas;

static Expected<std::shared_ptr<llvm::cas::CASDB>>
createCAS(const CASConfiguration &Config, bool CreateEmptyCASOnFailure) {
  if (Config.CASPath.empty())
    return llvm::cas::createInMemoryCAS();

  // Compute the path.
  SmallString<128> Storage;
  StringRef Path = Config.CASPath;
  if (Path == "auto") {
    getDefaultOnDiskCASPath(Storage);
    Path = Storage;
  }

  // FIXME: Pass on the actual error from the CAS.
  if (auto MaybeCAS =
          llvm::expectedToOptional(llvm::cas::createOnDiskCAS(Path)))
    return std::move(*MaybeCAS);

  if (CreateEmptyCASOnFailure)
    return createInMemoryCAS();

  return createStringError(std::make_error_code(std::errc::invalid_argument),
                           "cannot be initialized from \'" + Path + "\'");
}

Expected<std::shared_ptr<llvm::cas::CASDB>>
CASOptions::getOrCreateCAS(bool CreateEmptyCASOnFailure) const {
  if (Cache.Config.IsFrozen)
    return Cache.CAS;

  auto &CurrentConfig = static_cast<const CASConfiguration &>(*this);
  if (!Cache.CAS || CurrentConfig != Cache.Config) {
    Cache.Config = CurrentConfig;
    if (auto Err = createCAS(Cache.Config, CreateEmptyCASOnFailure)
                       .moveInto(Cache.CAS))
      return std::move(Err);
  }

  return Cache.CAS;
}

Expected<std::shared_ptr<llvm::cas::CASDB>>
CASOptions::getOrCreateCASAndHideConfig() {
  if (Cache.Config.IsFrozen)
    return Cache.CAS;

  auto CAS = getOrCreateCAS();
  if (!CAS)
    return CAS.takeError();
  assert(*CAS == Cache.CAS && "Expected CAS to be cached");

  // Freeze the CAS and wipe out the visible config to hide it from future
  // accesses. For example, future diagnostics cannot see this. Something that
  // needs direct access to the CAS configuration will need to be
  // scheduled/executed at a level that has access to the configuration.
  auto &CurrentConfig = static_cast<CASConfiguration &>(*this);
  CurrentConfig = CASConfiguration();
  CurrentConfig.IsFrozen = Cache.Config.IsFrozen = true;

  if (*CAS) {
    // Set the CASPath to the hash schema, since that leaks through CASContext's
    // API and is observable.
    CurrentConfig.CASPath = (*CAS)->getHashSchemaIdentifier().str();
  }

  return CAS;
}

void CASOptions::ensurePersistentCAS() {
  assert(!IsFrozen && "Expected to check for a persistent CAS before freezing");
  switch (getKind()) {
  case UnknownCAS:
      llvm_unreachable("Cannot ensure persistent CAS if it's unknown / frozen");
  case InMemoryCAS:
    CASPath = "auto";
    break;
  case OnDiskCAS:
    break;
  }
}

using llvm::yaml::IO;
using llvm::yaml::MappingTraits;

struct CASIDFileImpl {
  std::string CASPath;
  std::string CASID;
};

namespace llvm {
namespace yaml {

template <> struct MappingTraits<CASIDFileImpl> {
  static void mapping(IO &IO, CASIDFileImpl &Value) {
    IO.mapTag("!cas-file-v1", true);
    IO.mapRequired("cas-id", Value.CASID);
    IO.mapRequired("cas-path", Value.CASPath);
  }
};

} // namespace yaml
} // namespace llvm

void CASOptions::writeCASIDFile(raw_ostream &OS, const CASID &ID) const {
  CASIDFileImpl Output;
  raw_string_ostream SS(Output.CASID);
  ID.print(SS);
  // Use the CASPath from the cache as the frozen CASOption have the path wiped.
  Output.CASPath = Cache.Config.CASPath;
  yaml::Output Yout(OS);
  Yout << Output;
}

Expected<CASID> CASOptions::createFromCASIDFile(MemoryBufferRef Buf) {
  yaml::Input Yin(Buf.getBuffer());
  CASIDFileImpl FileIn;
  Yin >> FileIn;

  if (Yin.error())
    return errorCodeToError(Yin.error());

  if (IsFrozen && Cache.Config.CASPath != FileIn.CASPath)
    return createStringError(
        std::make_error_code(std::errc::invalid_argument),
        "CASIDFile has a different CASPath from the frozen CASOptions \'" +
            Cache.Config.CASPath + "\' vs. \'" + FileIn.CASPath + "\'");

  if (!IsFrozen)
    CASPath = FileIn.CASPath;

  auto CAS = getOrCreateCAS();
  if (!CAS)
    return CAS.takeError();

  return (*CAS)->parseID(FileIn.CASID);
}
