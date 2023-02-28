//===- ActionCache.cpp ------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/ActionCache.h"
#include "llvm/CAS/CASID.h"
#include "llvm/CAS/ObjectStore.h"
#include "llvm/Support/Errc.h"

using namespace llvm;
using namespace llvm::cas;

void ActionCache::anchor() {}

CacheKey::CacheKey(const CASID &ID) : Key(toStringRef(ID.getHash()).str()) {}
CacheKey::CacheKey(const ObjectProxy &Proxy)
    : CacheKey(Proxy.getCAS(), Proxy.getRef()) {}
CacheKey::CacheKey(const ObjectStore &CAS, const ObjectRef &Ref)
    : Key(toStringRef(CAS.getID(Ref).getHash())) {}

void ActionCacheMap::anchor() {}

std::vector<std::pair<std::string, ActionCacheMap::FutureValue>>
ActionCacheMap::getAllValuesAsync() {
  std::vector<std::string> OutputNames = getAllNames();
  std::vector<std::pair<std::string, FutureValue>> Futures;
  for (std::string &Name : OutputNames) {
    FutureValue Future = getValueAsync(Name);
    Futures.push_back({std::move(Name), std::move(Future)});
  }
  return Futures;
}

namespace {

/// An \c ActionCacheMap implementation that wraps a \c StringMap.
class DefaultActionCacheMap : public ActionCacheMap {
public:
  StringMap<ObjectRef> Mappings;

  DefaultActionCacheMap(StringMap<ObjectRef> Mappings)
      : Mappings(std::move(Mappings)) {}

  std::vector<std::string> getAllNames() final {
    std::vector<std::string> Names;
    for (const auto &Mapping : Mappings) {
      Names.push_back(std::string(Mapping.first()));
    }
    return Names;
  }

  FutureValue getValueAsync(StringRef Name) final {
    std::promise<Expected<std::optional<ObjectRef>>> Promise;
    auto FoundI = Mappings.find(Name);
    if (FoundI == Mappings.end()) {
      Promise.set_value(
          createStringError(llvm::errc::invalid_argument,
                            Name + " not part of action cache map"));
    } else {
      Promise.set_value(FoundI->second);
    }
    return Promise.get_future();
  }
};

} // namespace

Expected<std::optional<std::unique_ptr<ActionCacheMap>>>
ActionCache::getMap(const CacheKey &ActionKey, ObjectStore &CAS,
                    bool Globally) const {

  // Default implementation that creates a \c StringMap by reading a CAS object
  // that bundled all the strings in the data portion, in the same order as
  // their associated \c ObjectRef references.

  Optional<CASID> Value;
  if (Error E = get(ActionKey, Globally).moveInto(Value))
    return std::move(E);
  if (!Value)
    return std::nullopt;

  auto ValueRef = CAS.getReference(*Value);
  if (!ValueRef)
    return std::nullopt;
  Expected<ObjectProxy> Obj = CAS.getProxy(*ValueRef);
  if (!Obj)
    return Obj.takeError();

  StringMap<ObjectRef> Mappings;
  StringRef RemainingNamesBuf = Obj->getData();
  if (Error E = Obj->forEachReference([&](ObjectRef Ref) -> Error {
        StringRef Name = RemainingNamesBuf.data();
        assert(!Name.empty());
        assert(!Mappings.count(Name));
        Mappings.insert({Name, Ref});
        assert(Name.size() < RemainingNamesBuf.size());
        assert(RemainingNamesBuf[Name.size()] == 0);
        RemainingNamesBuf = RemainingNamesBuf.substr(Name.size() + 1);
        return Error::success();
      }))
    return std::move(E);

  return std::make_unique<DefaultActionCacheMap>(std::move(Mappings));
}

Error ActionCache::putMap(const CacheKey &ActionKey,
                          const StringMap<ObjectRef> &Mappings,
                          ObjectStore &CAS, bool Globally) {

  // Default implementation that creates a CAS object that bundles all the
  // strings in the data portion, in the same order as their associated
  // \c ObjectRef references.

  SmallVector<std::pair<StringRef, ObjectRef>> OrderedEntries;
  OrderedEntries.reserve(Mappings.size());
  for (const auto &Mapping : Mappings) {
    OrderedEntries.push_back({Mapping.first(), Mapping.second});
  }
  // Sort the entries so that the value object we store is independent of the
  // StringMap order.
  llvm::sort(OrderedEntries,
             [](const std::pair<StringRef, ObjectRef> &LHS,
                const std::pair<StringRef, ObjectRef> &RHS) -> bool {
               return LHS.first < RHS.first;
             });

  SmallString<128> NamesBuf;
  SmallVector<ObjectRef, 6> Refs;
  Refs.reserve(Mappings.size());
  for (const auto &Mapping : OrderedEntries) {
    assert(!Mapping.first.empty());
    assert(!Mapping.first.contains('\0'));
    NamesBuf += Mapping.first;
    NamesBuf.push_back(0);
    Refs.push_back(Mapping.second);
  }

  Expected<ObjectRef> Value = CAS.storeFromString(Refs, NamesBuf);
  if (!Value)
    return Value.takeError();
  return put(ActionKey, CAS.getID(*Value), Globally);
}
