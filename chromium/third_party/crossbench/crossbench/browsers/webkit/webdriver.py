# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from crossbench.browsers.safari.webdriver import SafariWebDriver


class WebKitWebDriver(SafariWebDriver):
  """Basic implementation for "raw" WebKit builds """
