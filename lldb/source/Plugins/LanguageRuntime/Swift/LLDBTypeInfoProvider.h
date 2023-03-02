
#ifndef liblldb_LLDBTypeInfoProvider_h_
#define liblldb_LLDBTypeInfoProvider_h_

#include "Plugins/TypeSystem/Swift/TypeSystemSwift.h"
#include "swift/RemoteInspection/TypeLowering.h"

namespace lldb_private {

class SwiftLanguageRuntimeImpl;
class TypeSystemSwift;
class CompilerType;

class LLDBTypeInfoProvider : public swift::remote::TypeInfoProvider {
  SwiftLanguageRuntimeImpl &m_runtime;
  TypeSystemSwift &m_typesystem;

public:
  LLDBTypeInfoProvider(SwiftLanguageRuntimeImpl &runtime,
                       TypeSystemSwift &typesystem)
      : m_runtime(runtime), m_typesystem(typesystem) {}

  const swift::reflection::TypeInfo *
  getTypeInfo(llvm::StringRef mangledName) override;

  const swift::reflection::TypeInfo *
  GetOrCreateTypeInfo(CompilerType clang_type);
};
} // namespace lldb_private
#endif
