// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/font_list.h"

#include "base/values.h"

namespace content {

scoped_ptr<ListValue> GetFontList_SlowBlocking() {
  return scoped_ptr<ListValue>(new ListValue);
}

}  // namespace content
