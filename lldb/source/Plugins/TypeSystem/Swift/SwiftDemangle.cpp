//===-- SwiftDemangle.cpp ---------------------------------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2020 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#include "SwiftDemangle.h"
#include "LRUCache.h"

namespace lldb_private {
namespace swift_demangle {

SharedDemangledNode GetCachedDemangledSymbol(ConstString name) {
  static LRUCache<SharedDemangledNode> g_shared_cache{10};
  auto factory = [name] {
    return SharedDemangledNode::FromMangledSymbol(name);
  };
#ifndef NDEBUG
  auto validation_result = factory();
#endif
  auto result = g_shared_cache.GetOrCreate(name, factory);
#ifndef NDEBUG
  assert(validation_result == result &&
         "Cached SharedDemangledNode isn't equal to a fresh one");
#endif
  return result;
}


} // namespace swift_demangle
} // namespace lldb_private
