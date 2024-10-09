//===-- LRUCache.h ----------------------------------------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2024 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_LRUCache_h_
#define liblldb_LRUCache_h_

#include "lldb/Utility/ConstString.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLFunctionalExtras.h"
#include <list>
#include <mutex>

namespace lldb_private {
namespace swift_demangle {

/// A thread-safe Least Recently Used (LRU) cache implementation.
///
/// This class template implements an LRU cache with a fixed capacity, which
/// supports insertion, retrieval, and a factory-based retrieval mechanism. It
/// ensures thread safety through the use of a mutex lock. Keys are always
/// `ConstString`
template <typename Value>
class LRUCache {
public:
  /// Constructs an instance with the specified capacity.
  LRUCache(size_t capacity) : m_capacity(capacity) {}

  /// Retrieves a value from the cache for the given key.
  /// If the key is found in the cache, the corresponding value is returned and
  /// the element is moved to the front of the LRU list, indicating it was
  /// recently used. If the key is not found, std::nullopt is returned
  std::optional<Value> Get(ConstString key) {
    std::lock_guard lock{m_mutex};
    auto map_it = m_map.find(key);
    if (map_it == m_map.end())
      return std::nullopt;
    const auto &map_value = map_it->second;
    MoveListElementToFront(map_value.list_it);
    return map_value.value;
  }

  /// Inserts a key-value pair into the cache.
  /// If the key already exists in the cache, its value is updated and the
  /// element is moved to the front of the LRU list. If the key does not exist,
  /// it is inserted into the cache. If the cache is at full capacity, the least
  /// recently used element is evicted to make space for the new element.
  void Put(ConstString key, const Value &value) {
    if (m_capacity == 0)
      return;
    std::lock_guard lock{m_mutex};
    auto map_it = m_map.find(key);
    if (map_it != m_map.end()) {
      auto &map_value = map_it->second;
      map_value.value = value;
      MoveListElementToFront(map_value.list_it);
    } else {
      InsertElement(key, value);
    }
  }

  /// Retrieves a value from the cache or creates it if not present.
  /// If the key is found in the cache, the corresponding value is returned and
  /// the element is moved to the front of the LRU list. If the key is not
  /// found, the given factory function is called to create a new value, which
  /// is then inserted into the cache before being returned.
  Value GetOrCreate(ConstString key, llvm::function_ref<Value()> factory) {
    if (m_capacity == 0)
      return factory();

    {
      std::lock_guard lock{m_mutex};
      if (auto map_it = m_map.find(key); map_it != m_map.end()) {
        const auto &map_value = map_it->second;
        MoveListElementToFront(map_value.list_it);
        return map_value.value;
      }
    }

    // Call `factory` with `m_mutex` unlocked
    const auto value = factory();

    {
      std::lock_guard lock{m_mutex};
      InsertElement(key, value);
    }

    return value;
  }

private:
  using List = std::list<ConstString>;
  struct MapValue {
    List::iterator list_it;
    Value value;
  };
  using Map = llvm::DenseMap<ConstString, MapValue>;

  void InsertElement(ConstString key, const Value &value) {
    assert(m_capacity > 0);

    if (m_list.size() >= m_capacity) {
      // Evict the least recent element
      auto key = m_list.back();
      m_list.pop_back();
      m_map.erase(key);
    }

    m_list.emplace_front(key);
    std::pair<typename Map::iterator, bool> emplace_result =
        m_map.try_emplace(key, MapValue{m_list.begin(), value});
    assert(emplace_result.second && "Element with this key already exists");
  }

  void MoveListElementToFront(typename List::iterator it) {
    m_list.splice(m_list.begin(), m_list, it);
  }

  size_t m_capacity;
  List m_list;
  Map m_map;
  std::mutex m_mutex;
};

} // namespace swift_demangle
} // namespace lldb_private

#endif
