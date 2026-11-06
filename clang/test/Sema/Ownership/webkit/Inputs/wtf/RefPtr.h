#pragma once

#include <wtf/Compiler.h>
#include <wtf/Ref.h>
#include <wtf/RawPtrTraits.h>

namespace WTF {

template<typename T, typename _PtrTraits = RawPtrTraits<T>, typename _RefDerefTraits = DefaultRefDerefTraits<T>>
class RefPtr {
// refptr-warning@-1 {{type 'WTF::RefPtr<WebCore::Document>' was inferred as 'self-contained'}}
public:
    using PtrTraits = _PtrTraits;
    using RefDerefTraits = _RefDerefTraits;

    ALWAYS_INLINE constexpr RefPtr() : m_ptr(nullptr) { }
    ALWAYS_INLINE RefPtr(T* ptr) : m_ptr(RefDerefTraits::refIfNotNull(ptr)) { }

    ALWAYS_INLINE ~RefPtr() { RefDerefTraits::derefIfNotNull(PtrTraits::exchange(m_ptr, nullptr)); }

    T* get() const LIFETIME_BOUND { return PtrTraits::unwrap(m_ptr); }

private:
    typename PtrTraits::StorageType m_ptr;
    // common-note@-1 {{holds managed raw-pointer field 'm_ptr'}}
} SWIFT_SELF_CONTAINED;

} // namespace WTF

using WTF::RefPtr;
