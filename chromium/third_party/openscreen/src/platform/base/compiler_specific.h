// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_COMPILER_SPECIFIC_H_
#define PLATFORM_BASE_COMPILER_SPECIFIC_H_

#ifdef NOINLINE
#define OSP_NOINLINE NOINLINE
#elif __has_cpp_attribute(clang::noinline)
#define OSP_NOINLINE [[clang::noinline]]
#elif __has_cpp_attribute(gnu::noinline)
#define OSP_NOINLINE [[gnu::noinline]]
#elif __has_cpp_attribute(msvc::noinline)
#define OSP_NOINLINE [[msvc::noinline]]
#else
#define OSP_NOINLINE __attribute__((noinline))
#endif

#endif  // PLATFORM_BASE_COMPILER_SPECIFIC_H_
