// RUN: rm -rf %t && split-file %s %t
// RUN: %clang_cc1 -std=c++2b -fsyntax-only -I %t -I %S/Inputs \
// RUN:     -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis \
// RUN:     -Wlifetime-safety-experimental-field-origin-escape \
// RUN:     %t/test.cpp -verify=expected,common,ref,rsrc -Wownership-inference

//--- WebKit/Platform/IPC/MessageReceiver.h
#pragma once
#include <wtf/AbstractRefCountedAndCanMakeWeakPtr.h>

namespace IPC {

class Connection;
class Decoder;

// Abstract base: pure-virtual message handler + protected virtual destructor;
// the abstract virtual ref()/deref() come from the AbstractRefCounted base.
class MessageReceiver : public AbstractRefCountedAndCanMakeWeakPtr<MessageReceiver> {
// expected-warning@-1 {{type 'IPC::MessageReceiver' was trivially inferred as 'self-contained'}}
public:
    virtual void didReceiveMessage(Connection&, Decoder&) = 0;

protected:
    virtual ~MessageReceiver() = default;
};

} // namespace IPC

//--- WebKit/GPUProcess/RemoteSharedResourceCache.h
#pragma once
#include <WebKit/Platform/IPC/MessageReceiver.h>
#include <wtf/Ref.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebKit {

class GPUConnectionToWebProcess;

class RemoteSharedResourceCache final : public ThreadSafeRefCounted<RemoteSharedResourceCache>, IPC::MessageReceiver {
// expected-warning@-1 {{type 'WebKit::RemoteSharedResourceCache' was inferred as 'self-contained'}}
public:
    static Ref<RemoteSharedResourceCache> create(GPUConnectionToWebProcess&);
    virtual ~RemoteSharedResourceCache();

    void ref() const final { ThreadSafeRefCounted::ref(); }
    void deref() const final { ThreadSafeRefCounted::deref(); }

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
};

} // namespace WebKit

//--- test.cpp
#include <WebKit/GPUProcess/RemoteSharedResourceCache.h>
#include <wtf/Ref.h>

using namespace WebKit;

template <class T> T get();
auto force_instantiate = get<Ref<RemoteSharedResourceCache>>();
