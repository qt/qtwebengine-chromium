/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebScopedPtr_h
#define WebScopedPtr_h

#include <stddef.h>

namespace WebTestRunner {

template<typename Deallocator, typename T>
class WebScopedPtrBase {
public:
    // Default constructor. Constructs an empty scoped pointer.
    inline WebScopedPtrBase() : m_ptr(0) { }

    // Constructs a scoped pointer from a plain one.
    explicit inline WebScopedPtrBase(T* ptr) : m_ptr(ptr) { }

    // Copy constructor removes the pointer from the original to avoid double
    // freeing.
    inline WebScopedPtrBase(const WebScopedPtrBase<Deallocator, T>& rhs)
        : m_ptr(rhs.m_ptr)
    {
        const_cast<WebScopedPtrBase<Deallocator, T>&>(rhs).m_ptr = 0;
    }

    // When the destructor of the scoped pointer is executed the plain pointer
    // is deleted using DeleteArray. This implies that you must allocate with
    // NewArray.
    inline ~WebScopedPtrBase()
    {
        if (m_ptr)
            Deallocator::Delete(m_ptr);
    }

    inline T* operator->() const { return m_ptr; }

    // You can get the underlying pointer out with the * operator.
    inline T* operator*() { return m_ptr; }

    // You can use [n] to index as if it was a plain pointer.
    inline T& operator[](size_t i)
    {
        return m_ptr[i];
    }

    // You can use [n] to index as if it was a plain pointer.
    const inline T& operator[](size_t i) const
    {
        return m_ptr[i];
    }

    // We don't have implicit conversion to a T* since that hinders migration:
    // You would not be able to change a method from returning a T* to
    // returning an WebScopedArrayPtr<T> and then get errors wherever it is used.
    inline T* get() const { return m_ptr; }

    inline void reset(T* newValue = 0)
    {
        if (m_ptr)
            Deallocator::Delete(m_ptr);
        m_ptr = newValue;
    }

    // Assignment requires an empty (0) WebScopedArrayPtr as the receiver. Like
    // the copy constructor it removes the pointer in the original to avoid
    // double freeing.
    inline WebScopedPtrBase<Deallocator, T>& operator=(const WebScopedPtrBase<Deallocator, T>& rhs)
    {
        reset(rhs.m_ptr);
        const_cast<WebScopedPtrBase<Deallocator, T>&>(rhs).m_ptr = 0;
        return *this;
    }

    inline bool isEmpty() { return !m_ptr; }

private:
    T* m_ptr;
};

// A 'scoped array pointer' that calls DeleteArray on its pointer when the
// destructor is called.
template<typename T>
struct ArrayDeallocator {
    static void Delete(T* array)
    {
        DeleteArray(array);
    }
};

template<typename T>
class WebScopedArrayPtr: public WebScopedPtrBase<ArrayDeallocator<T>, T> {
public:
    inline WebScopedArrayPtr() { }
    explicit inline WebScopedArrayPtr(T* ptr)
        : WebScopedPtrBase<ArrayDeallocator<T>, T>(ptr) { }
    inline WebScopedArrayPtr(const WebScopedArrayPtr<T>& rhs)
        : WebScopedPtrBase<ArrayDeallocator<T>, T>(rhs) { }
};

template<typename T>
struct ObjectDeallocator {
    static void Delete(T* object)
    {
        delete object;
    }
};

template<typename T>
class WebScopedPtr: public WebScopedPtrBase<ObjectDeallocator<T>, T> {
public:
    inline WebScopedPtr() { }
    explicit inline WebScopedPtr(T* ptr)
        : WebScopedPtrBase<ObjectDeallocator<T>, T>(ptr) { }
    inline WebScopedPtr(const WebScopedPtr<T>& rhs)
        : WebScopedPtrBase<ObjectDeallocator<T>, T>(rhs) { }
};

} // namespace WebTestRunner

#endif // WebScopedPtr_h
