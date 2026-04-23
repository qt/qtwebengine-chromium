// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBIPP_BINARY_CONTENT_H_
#define LIBIPP_BINARY_CONTENT_H_

#include <cstdint>
#include <string>
#include <vector>

// Helps build binary representation of frames for testing purposes.

struct BinaryContent {
  // frame content
  std::vector<uint8_t> data;
  // add ASCII string
  void s(std::string s);
  // add 1-byte integer
  void u1(int v);
  // add 2-bytes integer
  void u2(int v);
  // add 4-bytes integer
  void u4(int v);
};

#endif  //  LIBIPP_BINARY_CONTENT_H_
