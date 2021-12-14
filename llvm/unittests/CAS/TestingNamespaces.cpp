//===- TestingNamespaces.cpp ----------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TestingNamespaces.h"

using namespace llvm;
using namespace llvm::cas;

/// Helper for unit tests here to avoid depending on a specific CAS or hash
/// function.
const Namespace &testing::getNamespace(size_t HashSize, StringRef NameSuffix) {
  SmallString<128> Storage;
  StringRef Name = ("llvm.testing[" + Twine(HashSize) + "]" + NameSuffix).toStringRef();

  class TestingNamespace : Namespace {
  public:
    TestingNamespace(size_t HashSize, StringRef Name) : Namespace(Name, HashSize) {}
    void printIDImpl(const UniqueIDRef &ID, raw_ostream &OS) const override {
      hexadecimal::printHash(ID.getHash(), OS);
    }
    Error parseID(StringRef Printed, UniqueID &ID) const override {
      return hexadecimal::parseHashForID(*this, Printed, ID);
    }
  };

  static StringMap<std::unique_ptr<const TestingNamespace>> Map;
  static std::mutex Mutex;
  std::scoped_lock<std::mutex> Lock(Mutex);
  auto &NS = Map[Name];
  if (!NS)
    NS = std::make_unique<TestingNamespace>(HashSize, Name);
  return *NS;
}
