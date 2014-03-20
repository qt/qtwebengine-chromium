// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "stdafx.h"

#include "base/at_exit.h"
#include "testing/gtest/include/gtest/gtest.h"

#pragma comment(lib, "runtimeobject.lib")

base::AtExitManager at_exit;

int main(int argc, char** argv) {
  mswrw::RoInitializeWrapper ro_init(RO_INIT_SINGLETHREADED);

  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
