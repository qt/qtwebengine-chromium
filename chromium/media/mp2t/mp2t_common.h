// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MP2T_MP2T_COMMON_H_
#define MEDIA_MP2T_MP2T_COMMON_H_

#define LOG_LEVEL_TS  5
#define LOG_LEVEL_PES 4
#define LOG_LEVEL_ES  3

#define RCHECK(x) \
    do { \
      if (!(x)) { \
        DLOG(WARNING) << "Failure while parsing Mpeg2TS: " << #x; \
        return false; \
      } \
    } while (0)

#endif

