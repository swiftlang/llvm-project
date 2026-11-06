#pragma once

#include <wtf/Compiler.h>
#include <wtf/RawPtrTraits.h>

namespace WTF {

// doc-warning@+6         {{type 'WTF::DefaultRefDerefTraits<WebCore::Document>' was trivially inferred as 'self-contained'}}
// domparser-warning@+5   {{type 'WTF::DefaultRefDerefTraits<WebCore::DOMParser>' was trivially inferred as 'self-contained'}}
// eventsource-warning@+4 {{type 'WTF::DefaultRefDerefTraits<WebCore::EventSource>' was trivially inferred as 'self-contained'}}
// htmlspan-warning@+3    {{type 'WTF::DefaultRefDerefTraits<WebCore::HTMLSpanElement>' was trivially inferred as 'self-contained'}}
// rsrc-warning@+2        {{type 'WTF::DefaultRefDerefTraits<WebKit::RemoteSharedResourceCache>' was trivially inferred as 'self-contained'}}
// refptr-warning@+1      {{type 'WTF::DefaultRefDerefTraits<WebCore::Document>' was trivially inferred as 'self-contained'}}
template<typename T> struct DefaultRefDerefTraits {
    static constexpr bool isDefaultImplementation = true;

    static ALWAYS_INLINE T* refIfNotNull(T* ptr)
    {
        if (ptr)
            ptr->ref();
        return ptr;
    }

    static ALWAYS_INLINE T& ref(T& ref)
    {
        ref.ref();
        return ref;
    }

    static ALWAYS_INLINE void derefIfNotNull(T* ptr)
    {
        if (ptr)
            ptr->deref();
    }
};

template<typename T, typename _PtrTraits = RawPtrTraits<T>, typename RefDerefTraits = DefaultRefDerefTraits<T>>
class Ref {
// doc-warning@-1         {{type 'WTF::Ref<WebCore::Document>' was inferred as 'self-contained'}}
// domparser-warning@-2   {{type 'WTF::Ref<WebCore::DOMParser>' was inferred as 'self-contained'}}
// eventsource-warning@-3 {{type 'WTF::Ref<WebCore::EventSource>' was inferred as 'self-contained'}}
// htmlspan-warning@-4    {{type 'WTF::Ref<WebCore::HTMLSpanElement>' was inferred as 'self-contained'}}
// rsrc-warning@-5        {{type 'WTF::Ref<WebKit::RemoteSharedResourceCache>' was inferred as 'self-contained'}}
// domparser-warning@-6   {{type 'WTF::Ref<const WebCore::Settings>' failed to infer}}
public:
    using PtrTraits = _PtrTraits;
    static constexpr bool isRef = true;

    // 'Ref<const Settings>' is never destroyed, so its inline destructor is not
    // instantiated for that specialization and its effect on 'm_ptr' is unknown.
    // domparser-note@+1 {{destructor was not instantiated for this specialization}}
    ~Ref()
    {
        if (auto* ptr = PtrTraits::exchange(m_ptr, nullptr))
            RefDerefTraits::derefIfNotNull(ptr);
    }

    Ref(T& object) : m_ptr(&RefDerefTraits::ref(object)) { }
    Ref(const Ref& other) : m_ptr(&RefDerefTraits::ref(other.get())) { }

    T* operator->() const LIFETIME_BOUND { return PtrTraits::unwrap(m_ptr); }
    T& get() const LIFETIME_BOUND { return *PtrTraits::unwrap(m_ptr); }

private:
    typename PtrTraits::StorageType m_ptr;
    // ref-note@-1 {{holds managed raw-pointer field 'm_ptr'}}
} SWIFT_SELF_CONTAINED;

} // namespace WTF

using WTF::Ref;
