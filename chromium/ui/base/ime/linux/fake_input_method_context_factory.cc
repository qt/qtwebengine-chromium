// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/linux/fake_input_method_context_factory.h"

#include "ui/base/ime/linux/fake_input_method_context.h"

namespace ui {

FakeInputMethodContextFactory::FakeInputMethodContextFactory() {}

// Overriden from ui::LinuxInputMethodContextFactory

scoped_ptr<LinuxInputMethodContext>
FakeInputMethodContextFactory::CreateInputMethodContext(
    LinuxInputMethodContextDelegate* /* delegate */) const {
  return scoped_ptr<LinuxInputMethodContext>(new FakeInputMethodContext());
}

}  // namespace ui
