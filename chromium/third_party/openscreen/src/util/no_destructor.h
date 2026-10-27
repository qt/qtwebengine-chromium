// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_NO_DESTRUCTOR_H_
#define UTIL_NO_DESTRUCTOR_H_

#include <new>
#include <type_traits>
#include <utility>

namespace openscreen {

// Helper type to create a function-local static variable of type `T` when `T`
// has a non-trivial destructor. Storing a `T` in a `NoDestructor<T>` will
// prevent `~T()` from running, even when the variable goes out of scope.
//
// Useful when a variable has static storage duration but its type has a
// non-trivial destructor. Using a function-local static variable prevents
// global constructors, while using `NoDestructor<T>` prevents global
// destructors.
//
// ## Example Usage
//
// const std::string& GetDefaultText() {
//   // Required since `static const std::string` requires a global destructor.
//   static const openscreen::NoDestructor<std::string> s("Hello world!");
//   return *s;
// }
template <typename T>
class NoDestructor {
 public:
  static_assert(!(std::is_trivially_constructible_v<T> &&
                  std::is_trivially_destructible_v<T>),
                "T is trivially constructible and destructible; please use a "
                "constinit object of type T directly instead");

  static_assert(
      !std::is_trivially_destructible_v<T>,
      "T is trivially destructible; please use a function-local static "
      "of type T directly instead");

  // Not constexpr; just write static constexpr T x = ...; if the value should
  // be a constexpr.
  template <typename... Args>
  explicit NoDestructor(Args&&... args) {
    new (storage_) T(std::forward<Args>(args)...);
  }

  // Allows copy and move construction of the contained type, to allow
  // construction from an initializer list, e.g. for std::vector.
  explicit NoDestructor(const T& x) { new (storage_) T(x); }
  explicit NoDestructor(T&& x) { new (storage_) T(std::move(x)); }

  NoDestructor(const NoDestructor&) = delete;
  NoDestructor& operator=(const NoDestructor&) = delete;

  ~NoDestructor() = default;

  const T& operator*() const { return *get(); }
  T& operator*() { return *get(); }

  const T* operator->() const { return get(); }
  T* operator->() { return get(); }

  const T* get() const { return reinterpret_cast<const T*>(storage_); }
  T* get() { return reinterpret_cast<T*>(storage_); }

 private:
  alignas(T) char storage_[sizeof(T)];

#if defined(LEAK_SANITIZER)
  T* storage_ptr_ = reinterpret_cast<T*>(storage_);
#endif  // defined(LEAK_SANITIZER)
};

}  // namespace openscreen

#endif  // UTIL_NO_DESTRUCTOR_H_
