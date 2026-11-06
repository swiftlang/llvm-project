#pragma once

namespace WTF {

// Provides the abstract virtual ref()/deref() that derived refcounted types
// override `final` and forward to their actual refcounting base.
template<typename T>
class AbstractRefCountedAndCanMakeWeakPtr {
// rsrc-warning@-1 {{type 'WTF::AbstractRefCountedAndCanMakeWeakPtr<IPC::MessageReceiver>' was trivially inferred as 'self-contained'}}
public:
    virtual void ref() const = 0;
    virtual void deref() const = 0;

protected:
    virtual ~AbstractRefCountedAndCanMakeWeakPtr() = default;
};

} // namespace WTF

using WTF::AbstractRefCountedAndCanMakeWeakPtr;
