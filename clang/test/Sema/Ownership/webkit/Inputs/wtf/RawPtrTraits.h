#pragma once

#include <wtf/Compiler.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

// doc-warning@+7         {{type 'WTF::RawPtrTraits<WebCore::Document>' was trivially inferred as 'self-contained'}}
// refptr-warning@+6      {{type 'WTF::RawPtrTraits<WebCore::Document>' was trivially inferred as 'self-contained'}}
// domparser-warning@+5   {{type 'WTF::RawPtrTraits<WebCore::DOMParser>' was trivially inferred as 'self-contained'}}
// eventsource-warning@+4 {{type 'WTF::RawPtrTraits<WebCore::EventSource>' was trivially inferred as 'self-contained'}}
// htmlspan-warning@+3    {{type 'WTF::RawPtrTraits<WebCore::HTMLSpanElement>' was trivially inferred as 'self-contained'}}
// rsrc-warning@+2        {{type 'WTF::RawPtrTraits<WebKit::RemoteSharedResourceCache>' was trivially inferred as 'self-contained'}}
// domparser-warning@+1   {{type 'WTF::RawPtrTraits<const WebCore::Settings>' was trivially inferred as 'self-contained'}}
template<typename T> struct RawPtrTraits {
    template<typename U> using RebindTraits = RawPtrTraits<U>;

    using StorageType = T*;

    template<typename U>
    static ALWAYS_INLINE T* exchange(StorageType& ptr, U&& newValue) { return std::exchange(ptr, newValue); }

    static ALWAYS_INLINE T* unwrap(const StorageType& ptr) { return ptr; }
};

} // namespace WTF

using WTF::RawPtrTraits;
