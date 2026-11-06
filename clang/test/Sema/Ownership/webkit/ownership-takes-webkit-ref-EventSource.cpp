// RUN: rm -rf %t && split-file %s %t
// RUN: %clang_cc1 -std=c++2b -fsyntax-only -I %t -I %S/Inputs \
// RUN:     -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis \
// RUN:     -Wlifetime-safety-experimental-field-origin-escape \
// RUN:     %t/test.cpp -verify=expected,common,ref,eventsource -Wownership-inference

//--- WebCore/dom/EventTarget.h
#pragma once

namespace WebCore {

// EventTarget declares virtual ref()/deref(); derived refcounted types override
// them `final` and forward to their actual refcounting base.
class EventTarget {
// expected-warning@-1 {{type 'WebCore::EventTarget' was trivially inferred as 'self-contained'}}
public:
    virtual void ref() const = 0;
    virtual void deref() const = 0;

protected:
    virtual ~EventTarget() = default;
};

} // namespace WebCore

//--- WebCore/page/EventSource.h
#pragma once
#include <WebCore/dom/EventTarget.h>
#include <wtf/RefCounted.h>

namespace WebCore {

// expected-warning@+1 {{type 'WebCore::ActiveDOMObject' was trivially inferred as 'self-contained'}}
class ActiveDOMObject { };
// expected-warning@+1 {{type 'WebCore::ThreadableLoaderClient' was trivially inferred as 'self-contained'}}
class ThreadableLoaderClient { };

// expected-warning@+1 {{type 'WebCore::EventSource' was inferred as 'self-contained'}}
class EventSource final : public RefCounted<EventSource>, public EventTarget, private ThreadableLoaderClient, public ActiveDOMObject {
public:
    virtual ~EventSource();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }
};

} // namespace WebCore

//--- test.cpp
#include <WebCore/page/EventSource.h>
#include <wtf/Ref.h>

using namespace WebCore;

template <class T> T get();
auto force_instantiate = get<Ref<EventSource>>();
