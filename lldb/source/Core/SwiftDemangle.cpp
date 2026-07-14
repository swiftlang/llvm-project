//===-- SwiftDemangle.cpp -------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "lldb/Core/SwiftDemangle.h"

#ifdef LLDB_ENABLE_SWIFT

#include "lldb/Utility/ConstString.h"
#include "lldb/Utility/StreamString.h"

#include "swift/Demangling/Demangle.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

using namespace lldb_private;

// The equivalent of "$\u03C4_" (dollar tau underscore) and "\u03C4_" used to
// build static generic parameter names.
static const char *g_dollar_tau_underscore = u8"$\u03C4_";
static const char *g_tau_underscore = g_dollar_tau_underscore + 1;

namespace {

/// A NodePrinter that records where certain portions of the demangled name
/// begin and end. This is a translation-unit-local copy of the printer used by
/// the Swift plugins, so lldbCore only needs the standalone swiftDemangling
/// library.
class TrackingNodePrinter : public swift::Demangle::NodePrinter {
public:
  TrackingNodePrinter(swift::Demangle::DemangleOptions options)
      : swift::Demangle::NodePrinter(options) {}

  lldb_private::DemangledNameInfo takeInfo() { return std::move(info); }

private:
  lldb_private::DemangledNameInfo info;
  std::optional<unsigned> parametersDepth;
  std::optional<unsigned> genericsSignatureDepth;

  void startName() {
    if (!info.hasBasename())
      info.BasenameRange.first = getStreamLength();
  }

  void endName() {
    if (!info.hasBasename())
      info.BasenameRange.second = getStreamLength();
  }

  void startGenericSignature(unsigned depth) {
    if (genericsSignatureDepth || !info.hasBasename() ||
        info.TemplateArgumentsRange.first <
            info.TemplateArgumentsRange.second) {
      return;
    }
    info.TemplateArgumentsRange.first = getStreamLength();
    genericsSignatureDepth = depth;
  }

  void endGenericSignature(unsigned depth) {
    if (!genericsSignatureDepth || *genericsSignatureDepth != depth ||
        info.TemplateArgumentsRange.first <
            info.TemplateArgumentsRange.second) {
      return;
    }
    info.TemplateArgumentsRange.second = getStreamLength();
  }

  void startParameters(unsigned depth) {
    if (parametersDepth || !info.hasBasename() ||
        info.ArgumentsRange.first < info.ArgumentsRange.second) {
      return;
    }
    info.ArgumentsRange.first = getStreamLength();
    parametersDepth = depth;
  }

  void endParameters(unsigned depth) {
    if (!parametersDepth || *parametersDepth != depth ||
        info.ArgumentsRange.first < info.ArgumentsRange.second) {
      return;
    }
    info.ArgumentsRange.second = getStreamLength();
  }

  bool shouldTrackNameRange(swift::Demangle::NodePointer Node) const {
    using namespace swift::Demangle;
    switch (Node->getKind()) {
    case Node::Kind::Function:
    case Node::Kind::Constructor:
    case Node::Kind::Allocator:
    case Node::Kind::ExplicitClosure:
      return true;
    default:
      return false;
    }
  }

  void printFunctionName(bool hasName, llvm::StringRef &OverwriteName,
                         llvm::StringRef &ExtraName, bool MultiWordName,
                         int &ExtraIndex, swift::Demangle::NodePointer Entity,
                         unsigned int depth) override {
    if (shouldTrackNameRange(Entity))
      startName();
    NodePrinter::printFunctionName(hasName, OverwriteName, ExtraName,
                                   MultiWordName, ExtraIndex, Entity, depth);
    if (shouldTrackNameRange(Entity))
      endName();
  }

  void printGenericSignature(swift::Demangle::NodePointer Node,
                             unsigned depth) override {
    startGenericSignature(depth);
    NodePrinter::printGenericSignature(Node, depth);
    endGenericSignature(depth);
  }

  void printFunctionParameters(swift::Demangle::NodePointer LabelList,
                               swift::Demangle::NodePointer ParameterType,
                               unsigned depth, bool showTypes) override {
    startParameters(depth);
    NodePrinter::printFunctionParameters(LabelList, ParameterType, depth,
                                         showTypes);
    endParameters(depth);
  }
};

/// The installed hook used to resolve dynamic generic parameter names. Null
/// when no Swift language runtime plugin has been loaded (e.g. lldb-server).
SwiftDemangle::GenericParameterNameResolver g_generic_param_name_resolver =
    nullptr;

swift::Mangle::ManglingFlavor GetManglingFlavor(llvm::StringRef mangled_name) {
  if (mangled_name.starts_with("$e") || mangled_name.starts_with("_$e"))
    return swift::Mangle::ManglingFlavor::Embedded;
  return swift::Mangle::ManglingFlavor::Default;
}

static bool
ParseLocalDeclName(const swift::Demangle::NodePointer &node,
                   StreamString &identifier,
                   swift::Demangle::Node::Kind &parent_kind,
                   swift::Demangle::Node::Kind &kind) {
  for (auto *child : *node) {
    swift::Demangle::Node::Kind child_kind = child->getKind();
    switch (child_kind) {
    case swift::Demangle::Node::Kind::Number:
      break;

    default:
      if (child->hasText()) {
        identifier.PutCString(child->getText());
        return true;
      }
      break;
    }
  }
  return false;
}

static bool ParseFunction(const swift::Demangle::NodePointer &node,
                          StreamString &identifier,
                          swift::Demangle::Node::Kind &parent_kind,
                          swift::Demangle::Node::Kind &kind) {
  if (node->getNumChildren() >= 2) {
    // First child is the function's scope
    parent_kind = node->getChild(0)->getKind();
    // Second child is either the type (no identifier)
    auto *child2 = node->getChild(1);
    switch (child2->getKind()) {
    case swift::Demangle::Node::Kind::Type:
      break;

    case swift::Demangle::Node::Kind::LocalDeclName:
      if (ParseLocalDeclName(child2, identifier, parent_kind, kind))
        return true;
      else
        return false;
      break;

    default:
    case swift::Demangle::Node::Kind::InfixOperator:
    case swift::Demangle::Node::Kind::PostfixOperator:
    case swift::Demangle::Node::Kind::PrefixOperator:
    case swift::Demangle::Node::Kind::Identifier:
      if (child2->hasText())
        identifier.PutCString(child2->getText());
      return true;
    }
  }
  return false;
}

static bool ParseGlobal(const swift::Demangle::NodePointer &node,
                        StreamString &identifier,
                        swift::Demangle::Node::Kind &parent_kind,
                        swift::Demangle::Node::Kind &kind) {
  for (auto *child : *node) {
    if (child) {
      // Peel off static node.
      if (child->getKind() == swift::Demangle::Node::Kind::Static &&
          child->hasChildren())
        child = child->getFirstChild();

      kind = child->getKind();
      switch (child->getKind()) {
      case swift::Demangle::Node::Kind::Allocator:
        identifier.PutCString("__allocating_init");
        ParseFunction(child, identifier, parent_kind, kind);
        return true;

      case swift::Demangle::Node::Kind::Constructor:
        identifier.PutCString("init");
        ParseFunction(child, identifier, parent_kind, kind);
        return true;

      case swift::Demangle::Node::Kind::Deallocator:
        identifier.PutCString("__deallocating_deinit");
        ParseFunction(child, identifier, parent_kind, kind);
        return true;

      case swift::Demangle::Node::Kind::Destructor:
        identifier.PutCString("deinit");
        ParseFunction(child, identifier, parent_kind, kind);
        return true;

      case swift::Demangle::Node::Kind::Getter:
      case swift::Demangle::Node::Kind::Setter:
      case swift::Demangle::Node::Kind::Function:
        return ParseFunction(child, identifier, parent_kind, kind);

      // Ignore these, they decorate a function at the same level, but don't
      // contain any text
      case swift::Demangle::Node::Kind::ObjCAttribute:
        break;

      default:
        return false;
      }
    }
  }
  return false;
}

} // namespace

void SwiftDemangle::SetGenericParameterNameResolver(
    GenericParameterNameResolver resolver) {
  g_generic_param_name_resolver = resolver;
}

bool SwiftDemangle::IsSwiftMangledName(llvm::StringRef name) {
  // Old-style mangling uses a "_T" prefix. This can lead to false positives
  // with other symbols that just so happen to start with "_T". To prevent this,
  // only return true for select old-style mangled names. The known cases to are
  // ObjC classes and protocols. Classes are prefixed with either "_TtC" or
  // "_TtGC" (generic classes). Protocols are prefixed with "_TtP". Other "_T"
  // prefixed symbols are not considered to be Swift symbols.
  if (name.starts_with("_T"))
    return name.starts_with("_TtC") || name.starts_with("_TtGC") ||
           name.starts_with("_TtP");
  return swift::Demangle::isSwiftSymbol(name);
}

std::pair<std::string, std::optional<DemangledNameInfo>>
SwiftDemangle::DemangleSymbolAsString(llvm::StringRef symbol, DemangleMode mode,
                                      bool tracking, const SymbolContext *sc,
                                      const ExecutionContext *exe_ctx) {
  bool did_init = false;
  llvm::DenseMap<ArchetypePath, llvm::StringRef> dict;
  swift::Demangle::DemangleOptions options;
  switch (mode) {
  case eSimplified:
    options = swift::Demangle::DemangleOptions::SimplifiedUIDemangleOptions();
    options.ShowAsyncResumePartial = false;
    options.ShowClosureSignature = false;
    break;
  case eTypeName:
    options.DisplayModuleNames = true;
    options.ShowPrivateDiscriminators = false;
    options.DisplayExtensionContexts = false;
    options.DisplayLocalNameContexts = false;
    options.ShowFunctionArgumentTypes = true;
    break;
  case eDisplayTypeName:
    options = swift::Demangle::DemangleOptions::SimplifiedUIDemangleOptions();
    options.DisplayStdlibModule = false;
    options.DisplayObjCModule = false;
    options.QualifyEntities = true;
    options.DisplayModuleNames = true;
    options.DisplayLocalNameContexts = false;
    options.DisplayDebuggerGeneratedModule = false;
    options.ShowFunctionArgumentTypes = true;
    options.ShowClosureSignature = false;
    break;
  }

  if (sc) {
    // Resolve generic parameters in the current function.
    options.GenericParameterName = [&](uint64_t depth, uint64_t index) {
      if (!did_init) {
        // The heavy dynamic-type binding operation lives in the Swift language
        // runtime plugin. When it has installed its hook, use it; otherwise
        // fall back to the static generic parameter name.
        if (g_generic_param_name_resolver)
          g_generic_param_name_resolver(*sc, exe_ctx, GetManglingFlavor(symbol),
                                        dict);
        did_init = true;
      }
      auto it = dict.find({depth, index});
      if (it != dict.end())
        return it->second.str();
      return swift::Demangle::genericParameterName(depth, index);
    };
  } else {
    // Print generic parameter names.
    options.GenericParameterName = [&](uint64_t depth, uint64_t index) {
      std::string name;
      {
        llvm::raw_string_ostream s(name);
        s << g_tau_underscore << depth << '_' << index;
      }
      return name;
    };
  }
  if (tracking) {
    TrackingNodePrinter printer = TrackingNodePrinter(options);
    swift::Demangle::demangleSymbolAsString(symbol, printer);
    return std::pair<std::string, std::optional<DemangledNameInfo>>(
        printer.takeString(), printer.takeInfo());
  }
  return std::pair<std::string, std::optional<DemangledNameInfo>>(
      swift::Demangle::demangleSymbolAsString(symbol, options), std::nullopt);
}

std::string SwiftDemangle::DemangleSymbolAsString(
    llvm::StringRef symbol, DemangleMode mode, const SymbolContext *sc,
    const ExecutionContext *exe_ctx) {
  return DemangleSymbolAsString(symbol, mode, false, sc, exe_ctx).first;
}

std::pair<std::string, DemangledNameInfo>
SwiftDemangle::TrackedDemangleSymbolAsString(llvm::StringRef symbol,
                                             DemangleMode mode,
                                             const SymbolContext *sc,
                                             const ExecutionContext *exe_ctx) {
  auto demangledData = DemangleSymbolAsString(symbol, mode, true, sc, exe_ctx);
  return std::pair<std::string, DemangledNameInfo>(demangledData.first,
                                                   *demangledData.second);
}

bool SwiftDemangle::ExtractFunctionBasenameFromMangled(ConstString mangled,
                                                       ConstString &basename,
                                                       bool &is_method) {
  bool success = false;
  swift::Demangle::Node::Kind kind = swift::Demangle::Node::Kind::Global;
  swift::Demangle::Node::Kind parent_kind = swift::Demangle::Node::Kind::Global;
  if (mangled) {
    const char *mangled_cstr = mangled.GetCString();
    const size_t mangled_cstr_len = mangled.GetLength();

    if (mangled_cstr_len > 3) {
      llvm::StringRef mangled_ref(mangled_cstr, mangled_cstr_len);

      // Only demangle swift functions
      // This is a no-op right now for the new mangling, because you
      // have to demangle the whole name to figure this out anyway.
      // I'm leaving the test here in case we actually need to do this
      // only to functions.
      swift::Demangle::Context ctx;
      auto *node = ctx.demangleSymbolAsNode(mangled_ref);
      StreamString identifier;
      if (node) {
        switch (node->getKind()) {
        case swift::Demangle::Node::Kind::Global:
          success = ParseGlobal(node, identifier, parent_kind, kind);
          break;

        default:
          break;
        }

        if (!identifier.GetString().empty()) {
          basename = ConstString(identifier.GetString());
        }
      }
    }
  }
  if (success) {
    switch (kind) {
    case swift::Demangle::Node::Kind::Allocator:
    case swift::Demangle::Node::Kind::Constructor:
    case swift::Demangle::Node::Kind::Deallocator:
    case swift::Demangle::Node::Kind::Destructor:
      is_method = true;
      break;

    case swift::Demangle::Node::Kind::Getter:
    case swift::Demangle::Node::Kind::Setter:
      // don't handle getters and setters right now...
      return false;

    case swift::Demangle::Node::Kind::Function:
      switch (parent_kind) {
      case swift::Demangle::Node::Kind::BoundGenericClass:
      case swift::Demangle::Node::Kind::BoundGenericEnum:
      case swift::Demangle::Node::Kind::BoundGenericStructure:
      case swift::Demangle::Node::Kind::Class:
      case swift::Demangle::Node::Kind::Enum:
      case swift::Demangle::Node::Kind::Structure:
        is_method = true;
        break;

      default:
        break;
      }
      break;

    default:
      break;
    }
  }
  return success;
}

#endif // LLDB_ENABLE_SWIFT
