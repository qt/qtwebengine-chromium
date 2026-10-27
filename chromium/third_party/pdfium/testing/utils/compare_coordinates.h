// Copyright 2024 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_UTILS_COMPARE_COORDINATES_H_
#define TESTING_UTILS_COMPARE_COORDINATES_H_

#include "public/fpdfview.h"

struct RectDouble {
  double left;
  double top;
  double right;
  double bottom;
};

void CompareFS_RECTF(const FS_RECTF& val1, const FS_RECTF& val2);
void CompareFS_MATRIX(const FS_MATRIX& val1, const FS_MATRIX& val2);
void CompareFS_RECTF_Three_Places(const FS_RECTF& val1, const FS_RECTF& val2);
void CompareFS_RECT_DOUBLE_Three_Places(const RectDouble& val1,
                                        const RectDouble& val2);
void CompareFS_RECT_DOUBLE(const RectDouble& val1, const RectDouble& val2);

#endif  // TESTING_UTILS_COMPARE_COORDINATES_H_
