#pragma once
#include <wtf/Compiler.h>

namespace WebCore {

// EventTarget declares pure-virtual ref()/deref(). Node overrides them
// (Node::ref/deref are virtual and Document can mark its forwarders `final`).
// EventTarget's other bases are omitted as irrelevant here.
class EventTarget {
// common-warning@-1 {{type 'WebCore::EventTarget' was trivially inferred as 'self-contained'}}
public:
    virtual void ref() const = 0;
    virtual void deref() const = 0;

protected:
    virtual ~EventTarget() = default;
};

class Node : public EventTarget {
// common-warning@-1 {{type 'WebCore::Node' was inferred as 'self-contained'}}
public:
    ALWAYS_INLINE void ref() const { m_refCountAndParentBit += s_refCountIncrement; }

    ALWAYS_INLINE void deref() const
    {
        auto updatedRefCount = m_refCountAndParentBit - s_refCountIncrement;
        if (!updatedRefCount) {
            const_cast<Node&>(*this).removedLastRef();
            return;
        }
        m_refCountAndParentBit = updatedRefCount;
    }

    // Out-of-line in Node.cpp; where `delete this` actually happens.
    void removedLastRef() __attribute__((ownership_takes(res, 1)));

protected:
    ~Node() = default;

private:
    static constexpr unsigned s_refCountIncrement = 2;
    mutable unsigned m_refCountAndParentBit { s_refCountIncrement };
};

// common-warning@+1 {{type 'WebCore::ContainerNode' was inferred as 'self-contained'}}
class ContainerNode : public Node { };

} // namespace WebCore
