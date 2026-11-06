#pragma once
#include <wtf/Compiler.h>
#include <wtf/MainThread.h>

namespace WTF {

class ThreadSafeRefCountedBase {
// rsrc-warning@-1 {{type 'WTF::ThreadSafeRefCountedBase' was inferred as 'self-contained'}}
    WTF_MAKE_NONCOPYABLE(ThreadSafeRefCountedBase);
public:
    void ref() const { ++m_refCount; }
    bool hasOneRef() const { return m_refCount == 1; }
    unsigned refCount() const { return m_refCount; }

protected:
    ThreadSafeRefCountedBase() = default;
    ~ThreadSafeRefCountedBase() = default;

    // Returns true if the pointer should be freed.
    bool derefBase() const
    {
        if (m_refCount == 1)
            return true;
        --m_refCount;
        return false;
    }

private:
    mutable unsigned m_refCount { 1 };
};

template<class T, DestructionThread destructionThread = DestructionThread::Any>
class ThreadSafeRefCounted : public ThreadSafeRefCountedBase {
// rsrc-warning@-1 {{type 'WTF::ThreadSafeRefCounted<WebKit::RemoteSharedResourceCache>' was inferred as 'self-contained'}}
public:
    void deref() const
    {
        if (!derefBase())
            return;

        delete static_cast<const T*>(this);
    }

    void swiftRef() const { ref(); }

protected:
    ThreadSafeRefCounted() = default;
    ~ThreadSafeRefCounted() = default;
} SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT BASE_CLASS_SWIFT_SHARED_REFERENCE(.swiftRef, .deref);

} // namespace WTF

using WTF::ThreadSafeRefCounted;
