// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_STACK_FRAME
#define EXTENSIONS_COMMON_STACK_FRAME

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"

namespace extensions {

struct StackFrame {
  StackFrame();
  StackFrame(const StackFrame& frame);
  StackFrame(size_t line_number,
             size_t column_number,
             const base::string16& source,
             const base::string16& function);
  ~StackFrame();

  // Construct a stack frame from a reported plain-text frame.
  static scoped_ptr<StackFrame> CreateFromText(
      const base::string16& frame_text);

  bool operator==(const StackFrame& rhs) const;

  size_t line_number;
  size_t column_number;
  base::string16 source;
  base::string16 function;  // optional
};

typedef std::vector<StackFrame> StackTrace;

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_STACK_FRAME

