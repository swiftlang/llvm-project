// RUN: rm -rf %t && split-file %s %t
// RUN: %clang_cc1 -I %t -std=c++20 -fsyntax-only -finterop-type-inference -Rinterop-type-inference-direct-view -Rinterop-type-inference-direct-view-explain -verify %t/main.cpp

// Carve-outs of WebKit's WTF::Ref and WTF::RefPtr, each in its own
// header and reduced to what matters for direct view inference. main.cpp
// instantiates them for int and for classes.

//--- main.cpp
#include "RefPtr.h"
#include "RefCounted.h"

using WTF::RefPtr;

struct Object : WTF::RefCounted<Object> {
  static RefPtr<Object> create() { return WTF::adoptRef(new Object); }
  int value{7};
};

void useInt(int &x) {
  RefPtr<int> i(&x);
  i = nullptr;
}

void useObject() {
  RefPtr<Object> a = Object::create();
  RefPtr<Object> b = a;
  RefPtr<Object> c = std::move(b);
  WTF::Ref<Object> r = c.releaseNonNull();
  RefPtr<Object> d = std::move(r);
  d = nullptr;
}

// RefPtr<int> deletes its pointee in the reduced traits, so its members may
// invalidate m_ptr.
// expected-remark@RefPtr.h:10 {{'RefPtr<int>' is not a direct view type}}
// expected-note@RefPtr.h:26 {{this member function may invalidate 'm_ptr'}}

// The other records are not direct view types for less interesting reasons:
// expected-remark@* 0+ {{is not a direct view type}}
// expected-note@* 0+ {{no member function captures an owning pointer parameter in a field}}

//--- Common.h
#pragma once

#define ALWAYS_INLINE inline __attribute__((always_inline))
#define LIFETIME_BOUND [[clang::lifetimebound]]
#define ASSERT(x) do { if (!(x)) __builtin_trap(); } while (0)

namespace std {

using nullptr_t = decltype(nullptr);

template<typename T> struct remove_reference { using type = T; };
template<typename T> struct remove_reference<T&> { using type = T; };
template<typename T> struct remove_reference<T&&> { using type = T; };

template<typename T> constexpr typename remove_reference<T>::type&& move(T&& t) noexcept { return static_cast<typename remove_reference<T>::type&&>(t); }
template<typename T> constexpr T&& forward(typename remove_reference<T>::type& t) noexcept { return static_cast<T&&>(t); }
template<typename T> constexpr T&& forward(typename remove_reference<T>::type&& t) noexcept { return static_cast<T&&>(t); }

template<typename T, typename U = T> constexpr T exchange(T& obj, U&& newValue)
{
    T old = std::move(obj);
    obj = std::forward<U>(newValue);
    return old;
}
template<typename T> constexpr void swap(T& a, T& b)
{
    T tmp = std::move(a);
    a = std::move(b);
    b = std::move(tmp);
}

} // namespace std

namespace WTF {

template<typename T> struct RawPtrTraits {
    using StorageType = T*;
    template<typename U> static ALWAYS_INLINE T* exchange(StorageType& ptr, U&& newValue) { return std::exchange(ptr, std::forward<U>(newValue)); }
    static ALWAYS_INLINE void swap(StorageType& a, StorageType& b) { std::swap(a, b); }
    static ALWAYS_INLINE T* unwrap(const StorageType& ptr) { return ptr; }
};

template<typename T> struct DefaultRefDerefTraits {
    static ALWAYS_INLINE T* refIfNotNull(T* ptr)
    {
        if (ptr) [[likely]]
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
        if (ptr) [[likely]]
            ptr->deref();
    }
};

// int has no ref() and deref(). Specialize the traits so that a RefPtr<int>
// deletes the pointee, as an intrusive RefPtr does when the last reference
// goes away.
template<> struct DefaultRefDerefTraits<int> {
    static int* refIfNotNull(int* ptr) { return ptr; }
    static int& ref(int& object) { return object; }
    static void derefIfNotNull(int* ptr) { delete ptr; }
};

inline void adopted(const void*) { }

template<typename T, typename PtrTraits = RawPtrTraits<T>, typename RefDerefTraits = DefaultRefDerefTraits<T>> class Ref;
template<typename T, typename PtrTraits = RawPtrTraits<T>, typename RefDerefTraits = DefaultRefDerefTraits<T>> class RefPtr;
template<typename T, typename PtrTraits = RawPtrTraits<T>, typename RefDerefTraits = DefaultRefDerefTraits<T>> Ref<T, PtrTraits, RefDerefTraits> adoptRef(T&);
template<typename T, typename PtrTraits = RawPtrTraits<T>, typename RefDerefTraits = DefaultRefDerefTraits<T>> RefPtr<T, PtrTraits, RefDerefTraits> adoptRef(T*);

} // namespace WTF

//--- Ref.h
#pragma once

#include "Common.h"

namespace WTF {

// Reduced Ref (the non-nullable sibling of RefPtr).
template<typename T, typename _PtrTraits, typename RefDerefTraits>
class Ref {
public:
    using PtrTraits = _PtrTraits;

    ~Ref()
    {
        if (auto* ptr = PtrTraits::exchange(m_ptr, nullptr))
            RefDerefTraits::derefIfNotNull(ptr);
    }
    Ref(T& object) : m_ptr(&RefDerefTraits::ref(object)) { }
    Ref(const Ref& other) : m_ptr(&RefDerefTraits::ref(other.get())) { }
    template<typename X, typename Y> Ref(const Ref<X, Y>& other) : m_ptr(&RefDerefTraits::ref(other.get())) { }
    Ref(Ref&& other) : m_ptr(&other.leakRef()) { ASSERT(m_ptr); }
    template<typename X, typename Y> Ref(Ref<X, Y>&& other) : m_ptr(&other.leakRef()) { ASSERT(m_ptr); }

    Ref& operator=(const Ref& o) { Ref tmp = o; swap(tmp); return *this; }
    Ref& operator=(Ref&& o) { Ref tmp = std::move(o); swap(tmp); return *this; }
    template<typename X, typename Y, typename Z> void swap(Ref<X, Y, Z>& o) { PtrTraits::swap(m_ptr, o.m_ptr); }

    T& get() const LIFETIME_BOUND { ASSERT(m_ptr); return *PtrTraits::unwrap(m_ptr); }

    [[nodiscard]] T& leakRef()
    {
        ASSERT(m_ptr);
        return *PtrTraits::exchange(m_ptr, nullptr);
    }

private:
    friend Ref adoptRef<T, PtrTraits, RefDerefTraits>(T&);
    template<typename X, typename Y, typename Z> friend class Ref;

    enum AdoptTag { Adopt };
    Ref(T& object, AdoptTag) : m_ptr(&object) { }

    typename PtrTraits::StorageType m_ptr;
};

template<typename T, typename PtrTraits, typename RefDerefTraits>
inline Ref<T, PtrTraits, RefDerefTraits> adoptRef(T& reference)
{
    adopted(&reference);
    return Ref<T, PtrTraits, RefDerefTraits>(reference, Ref<T, PtrTraits, RefDerefTraits>::Adopt);
}

} // namespace WTF

//--- RefPtr.h
#pragma once

#include "Ref.h"

namespace WTF {

// RefPtr (from RefPtr.h), reduced to what matters for direct view inference:
// the constructors, the destructor and the members that may invalidate m_ptr.
template<typename T, typename _PtrTraits, typename _RefDerefTraits>
class RefPtr {
public:
    using PtrTraits = _PtrTraits;
    using RefDerefTraits = _RefDerefTraits;

    ALWAYS_INLINE constexpr RefPtr() : m_ptr(nullptr) { }
    ALWAYS_INLINE constexpr RefPtr(std::nullptr_t) : m_ptr(nullptr) { }
    ALWAYS_INLINE RefPtr(T* ptr) : m_ptr(RefDerefTraits::refIfNotNull(ptr)) { }
    ALWAYS_INLINE RefPtr(T& ptr) : m_ptr(&RefDerefTraits::ref(ptr)) { }
    ALWAYS_INLINE RefPtr(const RefPtr& o) : m_ptr(RefDerefTraits::refIfNotNull(PtrTraits::unwrap(o.m_ptr))) { }
    template<typename X, typename Y, typename Z> RefPtr(const RefPtr<X, Y, Z>& o) : m_ptr(RefDerefTraits::refIfNotNull(PtrTraits::unwrap(o.get()))) { }

    ALWAYS_INLINE RefPtr(RefPtr&& o) : m_ptr(o.leakRef()) { }
    template<typename X, typename Y, typename Z> RefPtr(RefPtr<X, Y, Z>&& o) : m_ptr(o.leakRef()) { }
    template<typename X, typename Y> RefPtr(Ref<X, Y>&&);

    ALWAYS_INLINE ~RefPtr() { RefDerefTraits::derefIfNotNull(PtrTraits::exchange(m_ptr, nullptr)); }

    T* get() const LIFETIME_BOUND { return PtrTraits::unwrap(m_ptr); }

    Ref<T> releaseNonNull() { ASSERT(m_ptr); Ref<T> tmp(adoptRef(*m_ptr)); m_ptr = nullptr; return tmp; }

    [[nodiscard]] T* leakRef();

    RefPtr& operator=(const RefPtr&);
    RefPtr& operator=(std::nullptr_t);

    template<typename X, typename Y, typename Z> void swap(RefPtr<X, Y, Z>&);

private:
    friend RefPtr adoptRef<T, PtrTraits, RefDerefTraits>(T*);
    template<typename X, typename Y, typename Z> friend class RefPtr;

    enum AdoptTag { Adopt };
    RefPtr(T* ptr, AdoptTag) : m_ptr(ptr) { }

    typename PtrTraits::StorageType m_ptr;
};

template<typename T, typename U, typename V>
template<typename X, typename Y>
inline RefPtr<T, U, V>::RefPtr(Ref<X, Y>&& reference)
    : m_ptr(&reference.leakRef())
{
}

template<typename T, typename U, typename V>
inline T* RefPtr<T, U, V>::leakRef()
{
    return U::exchange(m_ptr, nullptr);
}

template<typename T, typename U, typename V>
inline RefPtr<T, U, V>& RefPtr<T, U, V>::operator=(const RefPtr& o)
{
    RefPtr ptr = o;
    swap(ptr);
    return *this;
}

template<typename T, typename U, typename V>
inline RefPtr<T, U, V>& RefPtr<T, U, V>::operator=(std::nullptr_t)
{
    V::derefIfNotNull(U::exchange(m_ptr, nullptr));
    return *this;
}

template<class T, typename U, typename V>
template<typename X, typename Y, typename Z>
inline void RefPtr<T, U, V>::swap(RefPtr<X, Y, Z>& o)
{
    U::swap(m_ptr, o.m_ptr);
}

template<typename T, typename U, typename V>
inline RefPtr<T, U, V> adoptRef(T* p)
{
    adopted(p);
    return RefPtr<T, U, V>(p, RefPtr<T, U, V>::Adopt);
}

} // namespace WTF

//--- RefCounted.h
#pragma once

namespace WTF {

// A minimal RefCounted base, so that RefPtr can be instantiated for a class.
template<typename T> class RefCounted {
public:
    void ref() const { ++m_refCount; }
    void deref() const
    {
        if (!--m_refCount)
            delete static_cast<const T*>(this);
    }
protected:
    RefCounted() = default; // starts at 1, like WTF; adoptRef() takes that ref
    ~RefCounted() = default;
private:
    mutable unsigned m_refCount { 1 };
};

} // namespace WTF
