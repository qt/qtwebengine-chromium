// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_RAW_REF_H_
#define UTIL_RAW_REF_H_

#if defined(BUILD_WITH_CHROMIUM)

#include "partition_alloc/pointers/raw_ref.h"  // nogncheck

namespace openscreen {

template <typename T>
using raw_ref = ::base::raw_ref<T>;

}  // namespace openscreen

#else  // !defined(BUILD_WITH_CHROMIUM)

#include <type_traits>
#include <utility>

#include "util/raw_ptr.h"

namespace openscreen {

template <typename T>
class raw_ref;

namespace internal {
template <typename T>
struct is_raw_ref : std::false_type {};

template <typename T>
struct is_raw_ref<raw_ref<T> > : std::true_type {};
}  // namespace internal

template <typename T>
class OPENSCREEN_TRIVIAL_ABI raw_ref {
 public:
  OPENSCREEN_ALWAYS_INLINE constexpr explicit raw_ref(T& ref) noexcept
      : ptr_(&ref) {}

  template <typename U,
            typename = std::enable_if_t<
                !internal::is_raw_ref<std::decay_t<U> >::value &&
                std::is_convertible_v<U&, T&> > >
  OPENSCREEN_ALWAYS_INLINE constexpr explicit raw_ref(U& ref) noexcept
      : ptr_(&ref) {}

  OPENSCREEN_ALWAYS_INLINE constexpr raw_ref(const raw_ref& other) noexcept =
      default;
  OPENSCREEN_ALWAYS_INLINE constexpr raw_ref(raw_ref&& other) noexcept =
      default;

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U&, T&> > >
  OPENSCREEN_ALWAYS_INLINE constexpr raw_ref(const raw_ref<U>& other) noexcept
      : ptr_(other.ptr_) {}

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U&, T&> > >
  OPENSCREEN_ALWAYS_INLINE constexpr raw_ref(raw_ref<U>&& other) noexcept
      : ptr_(std::move(other.ptr_)) {}

  ~raw_ref() = default;

  OPENSCREEN_ALWAYS_INLINE constexpr raw_ref& operator=(
      const raw_ref& other) noexcept = default;
  OPENSCREEN_ALWAYS_INLINE constexpr raw_ref& operator=(
      raw_ref&& other) noexcept = default;

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U&, T&> > >
  OPENSCREEN_ALWAYS_INLINE constexpr raw_ref& operator=(
      const raw_ref<U>& other) noexcept {
    ptr_ = other.ptr_;
    return *this;
  }

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U&, T&> > >
  OPENSCREEN_ALWAYS_INLINE constexpr raw_ref& operator=(
      raw_ref<U>&& other) noexcept {
    ptr_ = std::move(other.ptr_);
    return *this;
  }

  OPENSCREEN_ALWAYS_INLINE constexpr T& get() const noexcept { return *ptr_; }
  OPENSCREEN_ALWAYS_INLINE constexpr T* operator->() const noexcept {
    return ptr_.get();
  }
  OPENSCREEN_ALWAYS_INLINE constexpr T& operator*() const noexcept {
    return *ptr_;
  }

  template <typename U>
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator==(
      const raw_ref& lhs,
      const raw_ref<U>& rhs) noexcept {
    return lhs.ptr_ == rhs.ptr_;
  }
  template <typename U>
  OPENSCREEN_ALWAYS_INLINE friend constexpr bool operator!=(
      const raw_ref& lhs,
      const raw_ref<U>& rhs) noexcept {
    return lhs.ptr_ != rhs.ptr_;
  }

 private:
  template <typename U>
  friend class raw_ref;

  raw_ptr<T> ptr_;
};

}  // namespace openscreen

#endif  // !defined(BUILD_WITH_CHROMIUM)

#endif  // UTIL_RAW_REF_H_
