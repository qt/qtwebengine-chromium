# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.core import browser_options
from telemetry.core.backends.chrome import cros_interface


def CreateChromeBrowserOptions(br_options):
  browser_type = br_options.browser_type

  # Unit tests.
  if not browser_type:
    return br_options

  if (cros_interface.IsRunningOnCrosDevice() or
      browser_type.startswith('cros')):
    return CrosBrowserOptions(br_options)

  if browser_type.startswith('android'):
    return AndroidBrowserOptions(br_options)

  return br_options


class ChromeBrowserOptions(browser_options.BrowserOptions):
  """Chrome-specific browser options."""

  def __init__(self, br_options):
    super(ChromeBrowserOptions, self).__init__()
    # Copy to self.
    self.__dict__.update(br_options.__dict__)


class CrosBrowserOptions(ChromeBrowserOptions):
  """ChromeOS-specific browser options."""

  def __init__(self, br_options):
    super(CrosBrowserOptions, self).__init__(br_options)


class AndroidBrowserOptions(ChromeBrowserOptions):
  """Android-specific browser options."""

  def __init__(self, br_options):
    super(AndroidBrowserOptions, self).__init__(br_options)
