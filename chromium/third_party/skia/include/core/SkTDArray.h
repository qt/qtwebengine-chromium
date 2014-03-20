
/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#ifndef SkTDArray_DEFINED
#define SkTDArray_DEFINED

#include "SkTypes.h"

template <typename T> class SK_API SkTDArray {
public:
    SkTDArray() {
        fReserve = fCount = 0;
        fArray = NULL;
#ifdef SK_DEBUG
        fData = NULL;
#endif
    }
    SkTDArray(const T src[], int count) {
        SkASSERT(src || count == 0);

        fReserve = fCount = 0;
        fArray = NULL;
#ifdef SK_DEBUG
        fData = NULL;
#endif
        if (count) {
            fArray = (T*)sk_malloc_throw(count * sizeof(T));
#ifdef SK_DEBUG
            fData = (ArrayT*)fArray;
#endif
            memcpy(fArray, src, sizeof(T) * count);
            fReserve = fCount = count;
        }
    }
    SkTDArray(const SkTDArray<T>& src) {
        fReserve = fCount = 0;
        fArray = NULL;
#ifdef SK_DEBUG
        fData = NULL;
#endif
        SkTDArray<T> tmp(src.fArray, src.fCount);
        this->swap(tmp);
    }
    ~SkTDArray() {
        sk_free(fArray);
    }

    SkTDArray<T>& operator=(const SkTDArray<T>& src) {
        if (this != &src) {
            if (src.fCount > fReserve) {
                SkTDArray<T> tmp(src.fArray, src.fCount);
                this->swap(tmp);
            } else {
                memcpy(fArray, src.fArray, sizeof(T) * src.fCount);
                fCount = src.fCount;
            }
        }
        return *this;
    }

    friend bool operator==(const SkTDArray<T>& a, const SkTDArray<T>& b) {
        return  a.fCount == b.fCount &&
                (a.fCount == 0 ||
                 !memcmp(a.fArray, b.fArray, a.fCount * sizeof(T)));
    }
    friend bool operator!=(const SkTDArray<T>& a, const SkTDArray<T>& b) {
        return !(a == b);
    }

    void swap(SkTDArray<T>& other) {
        SkTSwap(fArray, other.fArray);
#ifdef SK_DEBUG
        SkTSwap(fData, other.fData);
#endif
        SkTSwap(fReserve, other.fReserve);
        SkTSwap(fCount, other.fCount);
    }

    /** Return a ptr to the array of data, to be freed with sk_free. This also
        resets the SkTDArray to be empty.
     */
    T* detach() {
        T* array = fArray;
        fArray = NULL;
        fReserve = fCount = 0;
        SkDEBUGCODE(fData = NULL;)
        return array;
    }

    bool isEmpty() const { return fCount == 0; }

    /**
     *  Return the number of elements in the array
     */
    int count() const { return fCount; }

    /**
     *  return the number of bytes in the array: count * sizeof(T)
     */
    size_t bytes() const { return fCount * sizeof(T); }

    T*  begin() { return fArray; }
    const T*  begin() const { return fArray; }
    T*  end() { return fArray ? fArray + fCount : NULL; }
    const T*  end() const { return fArray ? fArray + fCount : NULL; }

    T&  operator[](int index) {
        SkASSERT(index < fCount);
        return fArray[index];
    }
    const T&  operator[](int index) const {
        SkASSERT(index < fCount);
        return fArray[index];
    }

    T&  getAt(int index)  {
        return (*this)[index];
    }
    const T&  getAt(int index) const {
        return (*this)[index];
    }

    void reset() {
        if (fArray) {
            sk_free(fArray);
            fArray = NULL;
#ifdef SK_DEBUG
            fData = NULL;
#endif
            fReserve = fCount = 0;
        } else {
            SkASSERT(fReserve == 0 && fCount == 0);
        }
    }

    void rewind() {
        // same as setCount(0)
        fCount = 0;
    }

    void setCount(int count) {
        if (count > fReserve) {
            this->growBy(count - fCount);
        } else {
            fCount = count;
        }
    }

    void setReserve(int reserve) {
        if (reserve > fReserve) {
            SkASSERT(reserve > fCount);
            int count = fCount;
            this->growBy(reserve - fCount);
            fCount = count;
        }
    }

    T* prepend() {
        this->growBy(1);
        memmove(fArray + 1, fArray, (fCount - 1) * sizeof(T));
        return fArray;
    }

    T* append() {
        return this->append(1, NULL);
    }
    T* append(int count, const T* src = NULL) {
        int oldCount = fCount;
        if (count)  {
            SkASSERT(src == NULL || fArray == NULL ||
                    src + count <= fArray || fArray + oldCount <= src);

            this->growBy(count);
            if (src) {
                memcpy(fArray + oldCount, src, sizeof(T) * count);
            }
        }
        return fArray + oldCount;
    }

    T* appendClear() {
        T* result = this->append();
        *result = 0;
        return result;
    }

    T* insert(int index) {
        return this->insert(index, 1, NULL);
    }
    T* insert(int index, int count, const T* src = NULL) {
        SkASSERT(count);
        SkASSERT(index <= fCount);
        size_t oldCount = fCount;
        this->growBy(count);
        T* dst = fArray + index;
        memmove(dst + count, dst, sizeof(T) * (oldCount - index));
        if (src) {
            memcpy(dst, src, sizeof(T) * count);
        }
        return dst;
    }

    void remove(int index, int count = 1) {
        SkASSERT(index + count <= fCount);
        fCount = fCount - count;
        memmove(fArray + index, fArray + index + count, sizeof(T) * (fCount - index));
    }

    void removeShuffle(int index) {
        SkASSERT(index < fCount);
        int newCount = fCount - 1;
        fCount = newCount;
        if (index != newCount) {
            memcpy(fArray + index, fArray + newCount, sizeof(T));
        }
    }

    int find(const T& elem) const {
        const T* iter = fArray;
        const T* stop = fArray + fCount;

        for (; iter < stop; iter++) {
            if (*iter == elem) {
                return (int) (iter - fArray);
            }
        }
        return -1;
    }

    int rfind(const T& elem) const {
        const T* iter = fArray + fCount;
        const T* stop = fArray;

        while (iter > stop) {
            if (*--iter == elem) {
                return iter - stop;
            }
        }
        return -1;
    }

    /**
     * Returns true iff the array contains this element.
     */
    bool contains(const T& elem) const {
        return (this->find(elem) >= 0);
    }

    /**
     * Copies up to max elements into dst. The number of items copied is
     * capped by count - index. The actual number copied is returned.
     */
    int copyRange(T* dst, int index, int max) const {
        SkASSERT(max >= 0);
        SkASSERT(!max || dst);
        if (index >= fCount) {
            return 0;
        }
        int count = SkMin32(max, fCount - index);
        memcpy(dst, fArray + index, sizeof(T) * count);
        return count;
    }

    void copy(T* dst) const {
        this->copyRange(dst, 0, fCount);
    }

    // routines to treat the array like a stack
    T*          push() { return this->append(); }
    void        push(const T& elem) { *this->append() = elem; }
    const T&    top() const { return (*this)[fCount - 1]; }
    T&          top() { return (*this)[fCount - 1]; }
    void        pop(T* elem) { if (elem) *elem = (*this)[fCount - 1]; --fCount; }
    void        pop() { --fCount; }

    void deleteAll() {
        T*  iter = fArray;
        T*  stop = fArray + fCount;
        while (iter < stop) {
            SkDELETE (*iter);
            iter += 1;
        }
        this->reset();
    }

    void freeAll() {
        T*  iter = fArray;
        T*  stop = fArray + fCount;
        while (iter < stop) {
            sk_free(*iter);
            iter += 1;
        }
        this->reset();
    }

    void unrefAll() {
        T*  iter = fArray;
        T*  stop = fArray + fCount;
        while (iter < stop) {
            (*iter)->unref();
            iter += 1;
        }
        this->reset();
    }

    void safeUnrefAll() {
        T*  iter = fArray;
        T*  stop = fArray + fCount;
        while (iter < stop) {
            SkSafeUnref(*iter);
            iter += 1;
        }
        this->reset();
    }

    void visitAll(void visitor(T&)) {
        T* stop = this->end();
        for (T* curr = this->begin(); curr < stop; curr++) {
            if (*curr) {
                visitor(*curr);
            }
        }
    }

#ifdef SK_DEBUG
    void validate() const {
        SkASSERT((fReserve == 0 && fArray == NULL) ||
                 (fReserve > 0 && fArray != NULL));
        SkASSERT(fCount <= fReserve);
        SkASSERT(fData == (ArrayT*)fArray);
    }
#endif

private:
#ifdef SK_DEBUG
    enum {
        kDebugArraySize = 16
    };
    typedef T ArrayT[kDebugArraySize];
    ArrayT* fData;
#endif
    T*      fArray;
    int     fReserve;
    int     fCount;

    void growBy(int extra) {
        SkASSERT(extra);

        if (fCount + extra > fReserve) {
            int size = fCount + extra + 4;
            size += size >> 2;

            fArray = (T*)sk_realloc_throw(fArray, size * sizeof(T));
#ifdef SK_DEBUG
            fData = (ArrayT*)fArray;
#endif
            fReserve = size;
        }
        fCount += extra;
    }
};

#endif
