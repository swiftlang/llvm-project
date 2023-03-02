#include "LLDBTypeInfoProvider.h"
#include "SwiftLanguageRuntimeImpl.h"

#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/Log.h"

#include "swift/RemoteInspection/TypeLowering.h"

using namespace lldb_private;

const swift::reflection::TypeInfo *
  LLDBTypeInfoProvider::getTypeInfo(llvm::StringRef mangledName) {
    // TODO: Should we cache the mangled name -> compiler type lookup, too?
    Log *log(GetLog(LLDBLog::Types));
    if (log)
      log->Printf("[LLDBTypeInfoProvider] Looking up debug type info for %s",
                  mangledName.str().c_str());

    // Materialize a Clang type from the debug info.
    assert(swift::Demangle::getManglingPrefixLength(mangledName) == 0);
    std::string wrapped;
    // The mangled name passed in is bare. Add global prefix ($s) and type (D).
    llvm::raw_string_ostream(wrapped) << "$s" << mangledName << 'D';
#ifndef NDEBUG
    {
      // Check that our hardcoded mangling wrapper is still up-to-date.
      swift::Demangle::Context dem;
      auto node = dem.demangleSymbolAsNode(wrapped);
      assert(node && node->getKind() == swift::Demangle::Node::Kind::Global);
      assert(node->getNumChildren() == 1);
      node = node->getChild(0);
      assert(node->getKind() == swift::Demangle::Node::Kind::TypeMangling);
      assert(node->getNumChildren() == 1);
      node = node->getChild(0);
      assert(node->getKind() == swift::Demangle::Node::Kind::Type);
      assert(node->getNumChildren() == 1);
      node = node->getChild(0);
      assert(node->getKind() != swift::Demangle::Node::Kind::Type);
    }
#endif
    ConstString mangled(wrapped);
    CompilerType swift_type = m_typesystem.GetTypeFromMangledTypename(mangled);
    auto ts = swift_type.GetTypeSystem().dyn_cast_or_null<TypeSystemSwift>();
    if (!ts)
      return nullptr;
    CompilerType clang_type;
    bool is_imported =
        ts->IsImportedType(swift_type.GetOpaqueQualType(), &clang_type);
    if (!is_imported || !clang_type) {
      if (log)
        log->Printf("[LLDBTypeInfoProvider] Could not find clang debug type info for %s",
                    mangledName.str().c_str());
      return nullptr;
    }
    
    return GetOrCreateTypeInfo(clang_type);
  }
  
  const swift::reflection::TypeInfo *
  LLDBTypeInfoProvider::GetOrCreateTypeInfo(CompilerType clang_type) {
    if (auto ti = m_runtime.lookupClangTypeInfo(clang_type))
      return *ti;

    auto &process = m_runtime.GetProcess();
    ExecutionContext exe_ctx;
    process.CalculateExecutionContext(exe_ctx);
    auto *exe_scope = exe_ctx.GetBestExecutionContextScope();
    // Build a TypeInfo for the Clang type.
    auto size = clang_type.GetByteSize(exe_scope);
    auto bit_align = clang_type.GetTypeBitAlign(exe_scope);
    std::vector<swift::reflection::FieldInfo> fields;
    if (clang_type.IsAggregateType()) {
      // Recursively collect TypeInfo records for all fields.
      for (uint32_t i = 0, e = clang_type.GetNumFields(&exe_ctx); i != e; ++i) {
        std::string name;
        uint64_t bit_offset_ptr = 0;
        uint32_t bitfield_bit_size_ptr = 0;
        bool is_bitfield_ptr = false;
        CompilerType field_type = clang_type.GetFieldAtIndex(
            i, name, &bit_offset_ptr, &bitfield_bit_size_ptr, &is_bitfield_ptr);
        if (is_bitfield_ptr) {
          Log *log(GetLog(LLDBLog::Types));
          if (log)
            log->Printf("[LLDBTypeInfoProvider] bitfield support is not yet "
                        "implemented");
          continue;
        }
        swift::reflection::FieldInfo field_info = {
            name, (unsigned)bit_offset_ptr / 8, 0, nullptr,
            *GetOrCreateTypeInfo(field_type)};
        fields.push_back(field_info);
      }
    }
    return m_runtime.emplaceClangTypeInfo(clang_type, size, bit_align, fields);
  }
