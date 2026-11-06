#pragma once
#include <wtf/Compiler.h>

namespace WTF {

class RefCountedBase {
// common-warning@-1 {{type 'WTF::RefCountedBase' was inferred as 'self-contained'}}
public:
    void ref() const { ++m_refCount; }
    bool hasOneRef() const { return m_refCount == 1; }
    unsigned refCount() const { return m_refCount; }

protected:
    RefCountedBase() = default;
    ~RefCountedBase() = default;

    // Returns true if the pointer should be deleted.
    bool derefBase() const
    {
        auto tempRefCount = m_refCount - 1;
        if (!tempRefCount)
            return true;
        m_refCount = tempRefCount;
        return false;
    }

private:
    mutable unsigned m_refCount { 1 };
};

template<typename T> class RefCounted : public RefCountedBase {
// domparser-warning@-1   {{type 'WTF::RefCounted<WebCore::DOMParser>' was inferred as 'self-contained'}}
// eventsource-warning@-2 {{type 'WTF::RefCounted<WebCore::EventSource>' was inferred as 'self-contained'}}
    WTF_MAKE_NONCOPYABLE(RefCounted);
public:
    void deref() const
    {
        if (derefBase())
            delete const_cast<T*>(static_cast<const T*>(this));
    }

    void swiftRef() const { ref(); }

protected:
    RefCounted() = default;
    ~RefCounted() = default;
} SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT BASE_CLASS_SWIFT_SHARED_REFERENCE(.swiftRef, .deref);

} // namespace WTF

using WTF::RefCounted;
