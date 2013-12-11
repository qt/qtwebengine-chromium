// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/test/test_suite.h"
#include "base/test/unit_test_launcher.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/views/view.h"

class ViewTestSuite : public base::TestSuite {
 public:
  ViewTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

 protected:
  virtual void Initialize() OVERRIDE {
    base::TestSuite::Initialize();
    ui::RegisterPathProvider();
    ui::ResourceBundle::InitSharedInstanceWithLocale("en-US", NULL);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ViewTestSuite);
};

int main(int argc, char** argv) {
  ViewTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv, base::Bind(&ViewTestSuite::Run,
                             base::Unretained(&test_suite)));
}
