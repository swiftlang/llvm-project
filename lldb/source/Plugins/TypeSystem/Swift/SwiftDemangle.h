//===-- SwiftDemangle.h ---------------------------------------*- C++ -*-===//
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

#ifndef liblldb_SwiftDemangle_h_
#define liblldb_SwiftDemangle_h_

#include "lldb/Utility/ConstString.h"
#include "swift/Demangling/Demangle.h"
#include "swift/Demangling/Demangler.h"
#include "llvm/ADT/ArrayRef.h"

namespace lldb_private {
namespace swift_demangle {

/// Access an inner node by following the given Node::Kind path.
///
/// Note: The Node::Kind path is relative to the given root node. The root
/// node's Node::Kind must not be included in the path.
inline swift::Demangle::NodePointer
nodeAtPath(swift::Demangle::NodePointer root,
           llvm::ArrayRef<swift::Demangle::Node::Kind> kind_path) {
  if (!root)
    return nullptr;

  auto *node = root;
  for (auto kind : kind_path) {
    bool found = false;
    for (auto *child : *node) {
      assert(child && "swift::Demangle::Node has null child");
      if (child && child->getKind() == kind) {
        node = child;
        found = true;
        break;
      }
    }
    // The current step (`kind`) of the path was not found in the children of
    // the current `node`. The requested path does not exist.
    if (!found)
      return nullptr;
  }

  return node;
}

/// \return the child of the \p Type node.
static swift::Demangle::NodePointer GetType(swift::Demangle::NodePointer n) {
  using namespace swift::Demangle;
  if (!n || n->getKind() != Node::Kind::Global)
    return nullptr;
  n = n->getFirstChild();
  if (!n || n->getKind() != Node::Kind::TypeMangling || !n->hasChildren())
    return nullptr;
  n = n->getFirstChild();
  if (!n || n->getKind() != Node::Kind::Type || !n->hasChildren())
    return nullptr;
  n = n->getFirstChild();
  return n;
}

/// Demangle a mangled type name and return the child of the \p Type node.
static inline swift::Demangle::NodePointer
GetDemangledType(swift::Demangle::Demangler &dem, llvm::StringRef name) {
  return GetType(dem.demangleSymbol(name));
}

/// Creates a copy of `node` using `dem` as a factory. The kind, text, and index
/// of the node are copied, but the children are not.
static inline NodePointer
CopyNodeWithoutChildren(swift::Demangle::Demangler &dem, const Node &node) {
  const auto kind = node.getKind();
  if (node.hasText())
    return dem.createNode(kind, node.getText());
  if (node.hasIndex())
    return dem.createNode(kind, node.getIndex());
  return dem.createNode(kind);
}

/// Creates a copy of `node` using `dem` as a factory. The kind, text, and index
/// of the node are fully copied, but the children are copied by reference
/// (shallow copy).
static inline NodePointer
CopyNodeWithChildrenReferences(swift::Demangle::Demangler &dem,
                               const Node &node) {
  NodePointer result = CopyNodeWithoutChildren(dem, node);
  if (!result)
    return nullptr;
  for (NodePointer child : node)
    result->addChild(child, dem);
  return result;
}

/// Shared ownership wrapper for swift::Demangle::NodePointer
///
/// Can be valid or invalid. A valid instance stores a non-null NodePointer and
/// a shared pointer to swift::Demangle::Demangler that owns this NodePointer.
/// An invalid instance has NodePointer equal to nullptr.
class SharedDemangledNode {
public:
  SharedDemangledNode(const SharedDemangledNode &) = default;

  SharedDemangledNode &operator=(const SharedDemangledNode &) = default;

  SharedDemangledNode(SharedDemangledNode &&) = default;

  SharedDemangledNode &operator=(SharedDemangledNode &&) = default;

  /// Constructs a SharedDemangledNode using a factory function.
  ///
  ///  This constructor creates a demangler and passes it to the factory
  ///  function.
  SharedDemangledNode(
      llvm::function_ref<NodePointer(swift::Demangle::Demangler &)> factory)
      : m_demangler(std::make_shared<swift::Demangle::Demangler>()) {
    m_node = factory(*m_demangler);
  }

  static SharedDemangledNode Invalid() {
    return SharedDemangledNode({}, nullptr);
  }

  /// Constructs a SharedDemangledNode from a mangled symbol.
  static SharedDemangledNode FromMangledSymbol(StringRef name) {
    return SharedDemangledNode([&](swift::Demangle::Demangler &demangler) {
      return demangler.demangleSymbol(name);
    });
  }

  explicit operator bool() const { return IsValid(); }

  bool operator==(const SharedDemangledNode &other) const {
    return Node::deepEquals(GetRawNode(), other.GetRawNode());
  }

  /// Gets a raw pointer to the Node.
  swift::Demangle::NodePointer GetRawNode() const & { return m_node; }

  /// This overload is intentionally deleted to prevent accidental use of the
  /// raw Node pointer associated with a temporary object. Use this method only
  /// on lvalues and don't let the pointer to escape the scope.
  swift::Demangle::NodePointer GetRawNode() && = delete;

  NodePointer operator->() const & {
    assert(m_node);
    return m_node;
  }

  /// This overload is intentionally deleted to prevent accidental use of the
  /// raw Node pointer associated with a temporary object. Use this operator
  /// only on lvalues.
  NodePointer operator->() && = delete;

  bool IsValid() const { return m_node != nullptr; }

  SharedDemangledNode GetType() const {
    if (!IsValid())
      return Invalid();
    if (auto *type_node = ::lldb_private::swift_demangle::GetType(m_node))
      return SharedDemangledNode(m_demangler, type_node);
    return Invalid();
  }

private:
  SharedDemangledNode(std::shared_ptr<swift::Demangle::Demangler> demangler,
                      swift::Demangle::NodePointer node)
      : m_demangler(demangler), m_node(node) {}

  std::shared_ptr<swift::Demangle::Demangler> m_demangler;
  swift::Demangle::NodePointer m_node;
};

// A container that holds a NodePointer along with additional dependencies.
//
// This class template is encapsulates a NodePointer and a variadic set of
// dependencies. It provides methods to access the NodePointer while ensuring
// that dependencies are held.
template <typename... Dependencies> class NodePointerWithDeps {
public:
  NodePointerWithDeps(NodePointer node, Dependencies... deps)
      : m_node(node), m_deps(std::make_tuple(std::move(deps)...)) {}

  explicit operator bool() const { return IsValid(); }

  /// Gets a raw pointer to the Node.
  swift::Demangle::NodePointer GetRawNode() const & { return m_node; }

  /// This overload is intentionally deleted to prevent accidental use of the
  /// raw Node pointer associated with a temporary object. Use this method only
  /// on lvalues and don't let the pointer to escape the scope.
  swift::Demangle::NodePointer GetRawNode() && = delete;

  NodePointer operator->() const & {
    assert(m_node);
    return m_node;
  }

  /// This overload is intentionally deleted to prevent accidental use of the
  /// raw Node pointer associated with a temporary object. Use this operator
  /// only on lvalues.
  NodePointer operator->() && = delete;

  bool IsValid() const { return m_node != nullptr; }

private:
  swift::Demangle::NodePointer m_node;
  std::tuple<Dependencies...> m_deps;
};

SharedDemangledNode GetCachedDemangledSymbol(ConstString name);

inline SharedDemangledNode GetCachedDemangledType(ConstString name) {
  return GetCachedDemangledSymbol(name).GetType();
}

/// Same as `demangleSymbolAsString` but uses the global demangling cache.
static inline std::string
DemangleSymbolAsString(ConstString mangled_name,
                       const DemangleOptions &options) {
  auto root = GetCachedDemangledSymbol(mangled_name);
  if (!root)
    return mangled_name.GetString();
  auto demangling = nodeToString(root.GetRawNode(), options);
  if (demangling.empty())
    return mangled_name.GetString();
  return demangling;
}

} // namespace swift_demangle
} // namespace lldb_private

#endif
