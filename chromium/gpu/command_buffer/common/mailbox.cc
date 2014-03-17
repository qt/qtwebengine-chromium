// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/mailbox.h"

#include <string.h>

#include "base/logging.h"

namespace gpu {

Mailbox::Mailbox() {
  memset(name, 0, sizeof(name));
}

bool Mailbox::IsZero() const {
  for (size_t i = 0; i < arraysize(name); ++i) {
    if (name[i])
      return false;
  }
  return true;
}

void Mailbox::SetZero() {
  memset(name, 0, sizeof(name));
}

void Mailbox::SetName(const int8* n) {
  DCHECK(IsZero() || !memcmp(name, n, sizeof(name)));
  memcpy(name, n, sizeof(name));
}

}  // namespace gpu
