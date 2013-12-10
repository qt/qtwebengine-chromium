# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import test

from measurements import tab_switching


class TabSwitchingTop10(test.Test):
  test = tab_switching.TabSwitching
  page_set = 'page_sets/top_10.json'
