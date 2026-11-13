// RUN: rm -rf %t && split-file %s %t
// RUN: %clang_cc1 -I %t -std=c++20 -fsyntax-only -finterop-type-inference -Rinterop-type-inference-direct-view -Rinterop-type-inference-direct-view-explain -verify %t/main.cpp

// Carve-outs of WebKit's WTF::Ref, WTF::RefPtr and WTF::Vector, each in its
// own header and reduced to what matters for direct view inference. main.cpp
// instantiates RefPtr for int and for a class, and Vector for char16_t.

//--- main.cpp
#include "RefPtr.h"
#include "RefCounted.h"
#include "Vector.h"

using WTF::RefPtr;

struct Object : WTF::RefCounted<Object> {
  int value{7};
};

void useInt(int &x) {
  RefPtr<int> i(&x);
  i = nullptr;
}

void useObject() {
  RefPtr<Object> a(new Object);
  a = nullptr;
}

void useVectors() {
  WTF::Vector<char16_t> heap;
  WTF::Vector<char16_t, 32> inlineStorage;
}

// VectorBufferBase is correctly a direct view: it captures the T* parameter in
// m_buffer. The buffers and vectors are not, because ~VectorBuffer reaches
// Malloc::free, which LSA takes as an invalidation although it is opaque.
// expected-remark@Vector.h:16 {{'VectorBufferBase<char16_t, WTF::FastMalloc>' is a direct view type}}
// expected-remark@Vector.h:52 {{'VectorBuffer<char16_t, 32>' is not a direct view type}}
// expected-note@Vector.h:60 {{this member function may invalidate 'VectorBufferBase<char16_t, WTF::FastMalloc>'}}
// expected-remark@Vector.h:73 {{'VectorBuffer<char16_t, 0>' is not a direct view type}}
// expected-note@Vector.h:78 {{this member function may invalidate 'VectorBufferBase<char16_t, WTF::FastMalloc>'}}
// expected-remark@Vector.h:85 {{'Vector<char16_t, 32>' is not a direct view type}}
// expected-remark@Vector.h:85 {{'Vector<char16_t>' is not a direct view type}}
// expected-note@Vector.h:85 2 {{no member function captures a pointer-to-self-contained parameter in a field}}

// RefPtr<int> deletes its pointee in the reduced traits, so its members may
// invalidate m_ptr.
// expected-remark@RefPtr.h:11 {{'RefPtr<int>' is not a direct view type}}
// expected-note@RefPtr.h:18 {{this member function may invalidate 'm_ptr'}}

// The other records are not direct view types for less interesting reasons:
// expected-remark@* 0+ {{is not a direct view type}}
// expected-note@* 0+ {{no member function captures a pointer-to-self-contained parameter in a field}}
// expected-note@* 0+ {{member function captures a pointer to a non-self-contained type in a field}}

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

// RefPtr (from RefPtr.h), reduced to what matters for direct view inference.
// Only the members that main.cpp uses: an uninstantiated member function
// disqualifies the record from the analysis.
template<typename T, typename _PtrTraits, typename _RefDerefTraits>
class RefPtr {
public:
    using PtrTraits = _PtrTraits;
    using RefDerefTraits = _RefDerefTraits;

    ALWAYS_INLINE RefPtr(T* ptr) : m_ptr(RefDerefTraits::refIfNotNull(ptr)) { }

    ALWAYS_INLINE ~RefPtr() { RefDerefTraits::derefIfNotNull(PtrTraits::exchange(m_ptr, nullptr)); }

    RefPtr& operator=(std::nullptr_t);

private:
    typename PtrTraits::StorageType m_ptr;
};

template<typename T, typename U, typename V>
inline RefPtr<T, U, V>& RefPtr<T, U, V>::operator=(std::nullptr_t)
{
    V::derefIfNotNull(U::exchange(m_ptr, nullptr));
    return *this;
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

//--- Vector.h
#pragma once

using size_t = decltype(sizeof(0));

namespace WTF {

// WebKit declares fastFree() and defines it in FastMalloc.cpp. free() is opaque
// here too: the analysis treats an opaque free(T*) as an invalidation.
struct FastMalloc {
    static void free(void* p);
};

// Vector, VectorBuffer and VectorBufferBase (from Vector.h), reduced to what
// matters for direct view inference.
template<typename T, typename Malloc>
class VectorBufferBase {
public:
    void deallocateBuffer(T* bufferToDeallocate)
    {
        if (!bufferToDeallocate)
            return;
        if (m_buffer == bufferToDeallocate) {
            m_buffer = nullptr;
            m_capacity = 0;
        }
        Malloc::free(bufferToDeallocate);
    }

    T* buffer() [[clang::lifetimebound]] { return m_buffer; }

protected:
    VectorBufferBase()
        : m_buffer(nullptr)
        , m_capacity(0)
        , m_size(0)
    {
    }

    VectorBufferBase(T* buffer, size_t capacity, size_t size)
        : m_buffer(buffer)
        , m_capacity(capacity)
        , m_size(size)
    {
    }

    T* m_buffer;
    unsigned m_capacity;
    unsigned m_size;
};

template<typename T, size_t inlineCapacity, typename Malloc = FastMalloc>
class VectorBuffer : private VectorBufferBase<T, Malloc> {
    typedef VectorBufferBase<T, Malloc> Base;
public:
    VectorBuffer()
        : Base(inlineBuffer(), inlineCapacity, 0)
    {
    }

    ~VectorBuffer()
    {
        if (Base::buffer() != inlineBuffer())
            Base::deallocateBuffer(Base::buffer());
    }

private:
    T* inlineBuffer() { return m_inlineBuffer; }

    T m_inlineBuffer[inlineCapacity];
};

template<typename T, typename Malloc>
class VectorBuffer<T, 0, Malloc> : private VectorBufferBase<T, Malloc> {
    typedef VectorBufferBase<T, Malloc> Base;
public:
    VectorBuffer() { }

    ~VectorBuffer()
    {
        Base::deallocateBuffer(Base::buffer());
    }
};

template<typename T, size_t inlineCapacity = 0>
class Vector : private VectorBuffer<T, inlineCapacity> {
public:
    Vector() { }
};

} // namespace WTF
