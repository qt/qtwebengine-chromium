// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_THREAD_ANNOTATIONS_H_
#define UTIL_THREAD_ANNOTATIONS_H_

#if defined(__clang__) && !defined(SWIG)
#define OSP_THREAD_ANNOTATION_ATTRIBUTE__(x) __attribute__((x))
#else
#define OSP_THREAD_ANNOTATION_ATTRIBUTE__(x)
#endif

#define OSP_GUARDED_BY(x) \
  OSP_THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))

#define OSP_PT_GUARDED_BY(x) \
  OSP_THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))

#define OSP_EXCLUSIVE_LOCKS_REQUIRED(...) \
  OSP_THREAD_ANNOTATION_ATTRIBUTE__(exclusive_locks_required(__VA_ARGS__))

#define OSP_SHARED_LOCKS_REQUIRED(...) \
  OSP_THREAD_ANNOTATION_ATTRIBUTE__(shared_locks_required(__VA_ARGS__))

#define OSP_EXCLUSIVE_LOCK_FUNCTION(...) \
  OSP_THREAD_ANNOTATION_ATTRIBUTE__(exclusive_lock_function(__VA_ARGS__))

#define OSP_SHARED_LOCK_FUNCTION(...) \
  OSP_THREAD_ANNOTATION_ATTRIBUTE__(shared_lock_function(__VA_ARGS__))

#define OSP_UNLOCK_FUNCTION(...) \
  OSP_THREAD_ANNOTATION_ATTRIBUTE__(unlock_function(__VA_ARGS__))

#define OSP_LOCKS_EXCLUDED(...) \
  OSP_THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(__VA_ARGS__))

#define OSP_LOCK_RETURNED(x) \
  OSP_THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))

#define OSP_LOCKABLE \
  OSP_THREAD_ANNOTATION_ATTRIBUTE__(lockable)

#define OSP_SCOPED_LOCKABLE \
  OSP_THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)

#define OSP_NO_THREAD_SAFETY_ANALYSIS \
  OSP_THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)

#endif  // UTIL_THREAD_ANNOTATIONS_H_
