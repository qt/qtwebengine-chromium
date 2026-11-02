// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_RAW_PTR_H_
#define UTIL_RAW_PTR_H_

// This header implements a conditional `raw_ptr` template.
//
// In Chromium builds (when `BUILD_WITH_CHROMIUM` is defined), it aliases
// Chromium's `base::raw_ptr` (MiraclePtr / BackupRefPtr). This allows Open
// Screen code to benefit from Chromium's UAF protection when running inside
// Chrome.
//
// In standalone builds (e.g., embedded/IoT builds where dependencies must be
// minimized and overhead must be zero), it provides a zero-overhead,
// dependency-free polyfill that behaves like a standard raw pointer but
// enforces initialization to `nullptr`.
//
// Note: Traits (like DanglingUntriaged or AllowPtrArithmetic) are intentionally
// not supported in Open Screen to ensure code safety and compatibility.

#if defined(BUILD_WITH_CHROMIUM)

#include "partition_alloc/pointers/raw_ptr.h"  // nogncheck

namespace openscreen {

// Alias the Chromium implementation, restricting it to not use traits.
template <typename T>
using raw_ptr = ::base::raw_ptr<T>;

}  // namespace openscreen

#else  // !defined(BUILD_WITH_CHROMIUM)

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <memory>
#include <type_traits>
#include <utility>

// Optimization macros to ensure the polyfill truly has zero overhead at the ABI
// level.
#if defined(__clang__)
#define OPENSCREEN_TRIVIAL_ABI [[clang::trivial_abi]]
#else
#define OPENSCREEN_TRIVIAL_ABI
#endif

#if defined(_MSC_VER)
#define OPENSCREEN_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define OPENSCREEN_ALWAYS_INLINE __attribute__((always_inline)) inline
#else
#define OPENSCREEN_ALWAYS_INLINE inline
#endif

namespace openscreen {

// Standalone polyfill for `raw_ptr`.
// It has zero runtime overhead compared to a raw pointer and compiles away.
template <typename T>
class OPENSCREEN_TRIVIAL_ABI raw_ptr {
 public:
  // Safety: auto-initialize to nullptr.
  OPENSCREEN_ALWAYS_INLINE constexpr raw_ptr() noexcept : ptr_(nullptr) {}

  OPENSCREEN_ALWAYS_INLINE constexpr raw_ptr(
      std::nullptr_t) noexcept  // NOLINT(runtime/explicit)
      : ptr_(nullptr) {}

  // Implicit conversion from raw pointer.
  OPENSCREEN_ALWAYS_INLINE constexpr raw_ptr(
      T* ptr) noexcept  // NOLINT(runtime/explicit)
      : ptr_(ptr) {}

  // Copy and Move constructors.
  OPENSCREEN_ALWAYS_INLINE constexpr raw_ptr(const raw_ptr& other) noexcept =
      default;

  OPENSCREEN_ALWAYS_INLINE constexpr raw_ptr(raw_ptr&& other) noexcept
      : ptr_(other.ptr_) {
    other.ptr_ = nullptr;
  }

  // Templated copy/move constructors for upcasting (Derived -> Base).
  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U*, T*> > >
  OPENSCREEN_ALWAYS_INLINE constexpr raw_ptr(
      const raw_ptr<U>& other) noexcept  // NOLINT(runtime/explicit)
      : ptr_(other.get()) {}

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U*, T*> > >
  OPENSCREEN_ALWAYS_INLINE constexpr raw_ptr(
      raw_ptr<U>&& other) noexcept  // NOLINT(runtime/explicit)
      : ptr_(other.ptr_) {
    other.ptr_ = nullptr;
  }

  // Destructor.
  OPENSCREEN_ALWAYS_INLINE constexpr ~raw_ptr() noexcept { ptr_ = nullptr; }

  // Assignment operators.
  OPENSCREEN_ALWAYS_INLINE constexpr raw_ptr& operator=(
      const raw_ptr& other) noexcept = default;

  OPENSCREEN_ALWAYS_INLINE constexpr raw_ptr& operator=(
      raw_ptr&& other) noexcept {
    if (this != &other) {
      ptr_ = other.ptr_;
      other.ptr_ = nullptr;
    }
    return *this;
  }

  OPENSCREEN_ALWAYS_INLINE constexpr raw_ptr& operator=(T* ptr) noexcept {
    ptr_ = ptr;
    return *this;
  }

  OPENSCREEN_ALWAYS_INLINE constexpr raw_ptr& operator=(
      std::nullptr_t) noexcept {
    ptr_ = nullptr;
    return *this;
  }

  // Templated assignment operators for upcasting.
  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U*, T*> > >
  OPENSCREEN_ALWAYS_INLINE constexpr raw_ptr& operator=(
      const raw_ptr<U>& other) noexcept {
    ptr_ = other.get();
    return *this;
  }

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U*, T*> > >
  OPENSCREEN_ALWAYS_INLINE constexpr raw_ptr& operator=(
      raw_ptr<U>&& other) noexcept {
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
    return *this;
  }

  // Pointer operations.
  OPENSCREEN_ALWAYS_INLINE constexpr T* get() const noexcept { return ptr_; }

  // Disable operator* for void types to prevent illegal void* dereferences and
  // void& signatures.
  template <typename U = T, typename = std::enable_if_t<!std::is_void_v<U> > >
  OPENSCREEN_ALWAYS_INLINE constexpr U& operator*() const noexcept {
    return *ptr_;
  }

  OPENSCREEN_ALWAYS_INLINE constexpr T* operator->() const noexcept {
    return ptr_;
  }

  OPENSCREEN_ALWAYS_INLINE constexpr operator T*() const noexcept {
    return ptr_;
  }

  OPENSCREEN_ALWAYS_INLINE constexpr raw_ptr& operator+=(
      ptrdiff_t delta) noexcept {
    ptr_ += delta;
    return *this;
  }

  OPENSCREEN_ALWAYS_INLINE constexpr raw_ptr& operator-=(
      ptrdiff_t delta) noexcept {
    ptr_ -= delta;
    return *this;
  }

  OPENSCREEN_ALWAYS_INLINE constexpr raw_ptr operator+(
      ptrdiff_t delta) const noexcept {
    return raw_ptr(ptr_ + delta);
  }

  OPENSCREEN_ALWAYS_INLINE constexpr raw_ptr operator-(
      ptrdiff_t delta) const noexcept {
    return raw_ptr(ptr_ - delta);
  }

  OPENSCREEN_ALWAYS_INLINE constexpr explicit operator bool() const noexcept {
    return ptr_ != nullptr;
  }

  // Swap helper.
  OPENSCREEN_ALWAYS_INLINE friend constexpr void swap(raw_ptr& lhs,
                                                      raw_ptr& rhs) noexcept {
    std::swap(lhs.ptr_, rhs.ptr_);
  }

  // Comparison operators (raw_ptr OP raw_ptr<U>).
  template <typename U>
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator==(
      const raw_ptr& lhs,
      const raw_ptr<U>& rhs) noexcept {
    return lhs.ptr_ == rhs.ptr_;
  }
  template <typename U>
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator!=(
      const raw_ptr& lhs,
      const raw_ptr<U>& rhs) noexcept {
    return lhs.ptr_ != rhs.ptr_;
  }
  template <typename U>
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator<(
      const raw_ptr& lhs,
      const raw_ptr<U>& rhs) noexcept {
    return lhs.ptr_ < rhs.ptr_;
  }
  template <typename U>
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator>(
      const raw_ptr& lhs,
      const raw_ptr<U>& rhs) noexcept {
    return lhs.ptr_ > rhs.ptr_;
  }
  template <typename U>
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator<=(
      const raw_ptr& lhs,
      const raw_ptr<U>& rhs) noexcept {
    return lhs.ptr_ <= rhs.ptr_;
  }
  template <typename U>
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator>=(
      const raw_ptr& lhs,
      const raw_ptr<U>& rhs) noexcept {
    return lhs.ptr_ >= rhs.ptr_;
  }

  // Comparison operators (raw_ptr OP U*).
  template <typename U>
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator==(const raw_ptr& lhs,
                                                            U* rhs) noexcept {
    return lhs.ptr_ == rhs;
  }
  template <typename U>
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator!=(const raw_ptr& lhs,
                                                            U* rhs) noexcept {
    return lhs.ptr_ != rhs;
  }
  template <typename U>
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator<(const raw_ptr& lhs,
                                                           U* rhs) noexcept {
    return lhs.ptr_ < rhs;
  }
  template <typename U>
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator>(const raw_ptr& lhs,
                                                           U* rhs) noexcept {
    return lhs.ptr_ > rhs;
  }
  template <typename U>
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator<=(const raw_ptr& lhs,
                                                            U* rhs) noexcept {
    return lhs.ptr_ <= rhs;
  }
  template <typename U>
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator>=(const raw_ptr& lhs,
                                                            U* rhs) noexcept {
    return lhs.ptr_ >= rhs;
  }

  // Comparison operators (U* OP raw_ptr).
  template <typename U>
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator==(
      U* lhs,
      const raw_ptr& rhs) noexcept {
    return lhs == rhs.ptr_;
  }
  template <typename U>
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator!=(
      U* lhs,
      const raw_ptr& rhs) noexcept {
    return lhs != rhs.ptr_;
  }
  template <typename U>
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator<(
      U* lhs,
      const raw_ptr& rhs) noexcept {
    return lhs < rhs.ptr_;
  }
  template <typename U>
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator>(
      U* lhs,
      const raw_ptr& rhs) noexcept {
    return lhs > rhs.ptr_;
  }
  template <typename U>
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator<=(
      U* lhs,
      const raw_ptr& rhs) noexcept {
    return lhs <= rhs.ptr_;
  }
  template <typename U>
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator>=(
      U* lhs,
      const raw_ptr& rhs) noexcept {
    return lhs >= rhs.ptr_;
  }

  // Comparison operators (raw_ptr OP nullptr).
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator==(
      const raw_ptr& lhs,
      std::nullptr_t) noexcept {
    return lhs.ptr_ == nullptr;
  }
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator!=(
      const raw_ptr& lhs,
      std::nullptr_t) noexcept {
    return lhs.ptr_ != nullptr;
  }
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator<(
      const raw_ptr& lhs,
      std::nullptr_t) noexcept {
    return lhs.ptr_ < nullptr;
  }
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator>(
      const raw_ptr& lhs,
      std::nullptr_t) noexcept {
    return lhs.ptr_ > nullptr;
  }
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator<=(
      const raw_ptr& lhs,
      std::nullptr_t) noexcept {
    return lhs.ptr_ <= nullptr;
  }
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator>=(
      const raw_ptr& lhs,
      std::nullptr_t) noexcept {
    return lhs.ptr_ >= nullptr;
  }

  // Comparison operators (nullptr OP raw_ptr).
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator==(
      std::nullptr_t,
      const raw_ptr& rhs) noexcept {
    return nullptr == rhs.ptr_;
  }
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator!=(
      std::nullptr_t,
      const raw_ptr& rhs) noexcept {
    return nullptr != rhs.ptr_;
  }
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator<(
      std::nullptr_t,
      const raw_ptr& rhs) noexcept {
    return nullptr < rhs.ptr_;
  }
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator>(
      std::nullptr_t,
      const raw_ptr& rhs) noexcept {
    return nullptr > rhs.ptr_;
  }
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator<=(
      std::nullptr_t,
      const raw_ptr& rhs) noexcept {
    return nullptr <= rhs.ptr_;
  }
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator>=(
      std::nullptr_t,
      const raw_ptr& rhs) noexcept {
    return nullptr >= rhs.ptr_;
  }

  // Stream output helper.
  template <typename CharT, typename Traits>
  OPENSCREEN_ALWAYS_INLINE friend std::basic_ostream<CharT, Traits>& operator<<(
      std::basic_ostream<CharT, Traits>& os,
      const raw_ptr& ptr) {
    return os << ptr.ptr_;
  }

 private:
  template <typename U>
  friend class raw_ptr;

#if defined(__clang__)
  [[clang::annotate("raw_ptr_exclusion")]]
#endif
  T* ptr_ = nullptr;
};

}  // namespace openscreen

namespace std {

// Override so map/set lookups work correctly.
template <typename T>
struct less<openscreen::raw_ptr<T> > {
  using is_transparent = void;

  bool operator()(const openscreen::raw_ptr<T>& lhs,
                  const openscreen::raw_ptr<T>& rhs) const {
    return lhs < rhs;
  }
  bool operator()(T* lhs, const openscreen::raw_ptr<T>& rhs) const {
    return lhs < rhs.get();
  }
  bool operator()(const openscreen::raw_ptr<T>& lhs, T* rhs) const {
    return lhs.get() < rhs;
  }
};

// Override so unordered_map/unordered_set lookups work correctly.
template <typename T>
struct hash<openscreen::raw_ptr<T> > {
  using argument_type = openscreen::raw_ptr<T>;
  using result_type = std::size_t;
  result_type operator()(argument_type const& ptr) const {
    return hash<T*>()(ptr.get());
  }
};

// Required for algorithms like std::to_address to unpack the pointer.
template <typename T>
struct pointer_traits<openscreen::raw_ptr<T> > {
  using pointer = openscreen::raw_ptr<T>;
  using element_type = T;
  using difference_type = ptrdiff_t;

  template <typename U>
  using rebind = openscreen::raw_ptr<U>;

  static constexpr pointer pointer_to(element_type& r) noexcept {
    return pointer(&r);
  }
  static constexpr element_type* to_address(pointer p) noexcept {
    return p.get();
  }
};

}  // namespace std

#endif  // !defined(BUILD_WITH_CHROMIUM)

#endif  // UTIL_RAW_PTR_H_
