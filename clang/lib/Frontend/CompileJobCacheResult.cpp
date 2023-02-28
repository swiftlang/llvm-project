//===- CompileJobCacheResult.cpp ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Frontend/CompileJobCacheResult.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/CAS/ObjectStore.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::cas;
using namespace llvm::cas;
using llvm::Error;

ArrayRef<CompileJobCacheResult::OutputKind>
CompileJobCacheResult::getAllOutputKinds() {
  static const OutputKind OutputKinds[] = {OutputKind::MainOutput,
                                           OutputKind::SerializedDiagnostics,
                                           OutputKind::Dependencies};
  return ArrayRef(OutputKinds);
}

Error CompileJobCacheResult::forEachOutput(
    llvm::function_ref<Error(Output)> Callback) const {
  size_t Count = getNumOutputs();
  for (size_t I = 0; I < Count; ++I) {
    OutputKind Kind = getOutputKind(I);
    ObjectRef Object = getOutputObject(I);
    if (auto Err = Callback({Object, Kind}))
      return Err;
  }
  return Error::success();
}

std::optional<CompileJobCacheResult::Output>
CompileJobCacheResult::getOutput(OutputKind Kind) const {
  size_t Count = getNumOutputs();
  for (size_t I = 0; I < Count; ++I) {
    OutputKind K = getOutputKind(I);
    if (Kind == K)
      return Output{getOutputObject(I), Kind};
  }
  return std::nullopt;
}

static constexpr llvm::StringLiteral MainOutputKindName = "main";
static constexpr llvm::StringLiteral SerializedDiagnosticsKindName = "diags";
static constexpr llvm::StringLiteral DependenciesOutputKindName = "deps";

StringRef CompileJobCacheResult::getOutputKindName(OutputKind Kind) {
  switch (Kind) {
  case OutputKind::MainOutput:
    return MainOutputKindName;
  case OutputKind::SerializedDiagnostics:
    return SerializedDiagnosticsKindName;
  case OutputKind::Dependencies:
    return DependenciesOutputKindName;
  }
}

std::optional<CompileJobCacheResult::OutputKind>
CompileJobCacheResult::getOutputKindForName(StringRef Name) {
  return llvm::StringSwitch<std::optional<OutputKind>>(Name)
      .Case(MainOutputKindName, OutputKind::MainOutput)
      .Case(SerializedDiagnosticsKindName, OutputKind::SerializedDiagnostics)
      .Case(DependenciesOutputKindName, OutputKind::Dependencies)
      .Default(std::nullopt);
}

static void printOutputKind(llvm::raw_ostream &OS,
                            CompileJobCacheResult::OutputKind Kind) {
  OS << CompileJobCacheResult::getOutputKindName(Kind) << "  ";
}

Error CompileJobCacheResult::print(llvm::raw_ostream &OS,
                                   const ObjectStore &CAS) {
  return forEachOutput([&](Output O) -> Error {
    printOutputKind(OS, O.Kind);
    OS << ' ' << CAS.getID(O.Object) << '\n';
    return Error::success();
  });
}

size_t CompileJobCacheResult::getNumOutputs() const { return Outputs.size(); }
ObjectRef CompileJobCacheResult::getOutputObject(size_t I) const {
  return Outputs[I].second;
}
CompileJobCacheResult::OutputKind
CompileJobCacheResult::getOutputKind(size_t I) const {
  return Outputs[I].first;
}

CompileJobCacheResult::CompileJobCacheResult(
    const llvm::StringMap<ObjectRef> &Mappings) {
  for (const auto &Mapping : Mappings) {
    auto Kind = getOutputKindForName(Mapping.first());
    if (!Kind)
      llvm::report_fatal_error("unknown output kind: " + Mapping.first());
    Outputs.emplace_back(*Kind, Mapping.second);
  }
}

struct CompileJobCacheResult::Builder::PrivateImpl {
  SmallVector<ObjectRef> Objects;
  SmallVector<OutputKind> Kinds;

  struct KindMap {
    OutputKind Kind;
    std::string Path;
  };
  SmallVector<KindMap> KindMaps;
};

CompileJobCacheResult::Builder::Builder() : Impl(*new PrivateImpl) {}
CompileJobCacheResult::Builder::~Builder() { delete &Impl; }

void CompileJobCacheResult::Builder::addKindMap(OutputKind Kind,
                                                StringRef Path) {
  Impl.KindMaps.push_back({Kind, std::string(Path)});
}
void CompileJobCacheResult::Builder::addOutput(OutputKind Kind,
                                               ObjectRef Object) {
  Impl.Kinds.push_back(Kind);
  Impl.Objects.push_back(Object);
}
Error CompileJobCacheResult::Builder::addOutput(StringRef Path,
                                                ObjectRef Object) {
  for (auto &KM : Impl.KindMaps) {
    if (KM.Path == Path) {
      addOutput(KM.Kind, Object);
      return Error::success();
    }
  }
  return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                 "cached output file has unknown path '" +
                                     Path + "'");
}

llvm::StringMap<ObjectRef> CompileJobCacheResult::Builder::build() {
  assert(Impl.Kinds.size() == Impl.Objects.size());
  llvm::StringMap<ObjectRef> Mappings;
  for (unsigned I = 0, E = Impl.Kinds.size(); I != E; ++I) {
    Mappings.insert({getOutputKindName(Impl.Kinds[I]), Impl.Objects[I]});
  }
  return Mappings;
}
