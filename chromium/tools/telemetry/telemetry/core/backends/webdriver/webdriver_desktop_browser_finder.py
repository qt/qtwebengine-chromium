# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Finds desktop browsers that can be controlled by telemetry."""

import logging
import os
import sys

from telemetry.core import browser
from telemetry.core import possible_browser
from telemetry.core import platform
from telemetry.core import util
from telemetry.core.backends.webdriver import webdriver_ie_backend
from telemetry.page import cloud_storage

# Try to import the selenium python lib which may be not available.
util.AddDirToPythonPath(
    util.GetChromiumSrcDir(), 'third_party', 'webdriver', 'pylib')
try:
  from selenium import webdriver  # pylint: disable=F0401
except ImportError:
  webdriver = None

ALL_BROWSER_TYPES = ''
if webdriver:
  ALL_BROWSER_TYPES = ','.join([
      'internet-explorer',
      'internet-explorer-x64'])
else:
  logging.warning('Webdriver backend is unsupported without selenium pylib. '
                  'For installation of selenium pylib, please refer to '
                  'https://code.google.com/p/selenium/wiki/PythonBindings.')


class PossibleWebDriverBrowser(possible_browser.PossibleBrowser):
  """A browser that can be controlled through webdriver API."""

  def __init__(self, browser_type, finder_options):
    super(PossibleWebDriverBrowser, self).__init__(browser_type, finder_options)

  def CreateWebDriverBackend(self, platform_backend):
    raise NotImplementedError()

  def Create(self):
    platform_backend = platform.CreatePlatformBackendForCurrentOS()
    backend = self.CreateWebDriverBackend(platform_backend)
    b = browser.Browser(backend, platform_backend)
    return b

  def SupportsOptions(self, finder_options):
    if len(finder_options.extensions_to_load) != 0:
      return False
    return True

  @property
  def last_modification_time(self):
    return -1


class PossibleDesktopIE(PossibleWebDriverBrowser):
  def __init__(self, browser_type, finder_options, architecture):
    super(PossibleDesktopIE, self).__init__(browser_type, finder_options)
    self._architecture = architecture

  def CreateWebDriverBackend(self, platform_backend):
    assert webdriver
    def DriverCreator():
      ie_driver_exe = os.path.join(util.GetTelemetryDir(), 'bin',
                                   'IEDriverServer_%s.exe' % self._architecture)
      cloud_storage.GetIfChanged(cloud_storage.PUBLIC_BUCKET, ie_driver_exe)
      return webdriver.Ie(executable_path=ie_driver_exe)
    return webdriver_ie_backend.WebDriverIEBackend(
        platform_backend, DriverCreator, self.finder_options.browser_options)

def SelectDefaultBrowser(_):
  return None

def FindAllAvailableBrowsers(finder_options):
  """Finds all the desktop browsers available on this machine."""
  browsers = []
  if not webdriver:
    return browsers

  # Look for the IE browser in the standard location.
  if sys.platform.startswith('win'):
    ie_path = os.path.join('Internet Explorer', 'iexplore.exe')
    win_search_paths = {
        '32' : { 'path' : os.getenv('PROGRAMFILES(X86)'),
                 'type' : 'internet-explorer'},
        '64' : { 'path' : os.getenv('PROGRAMFILES'),
                 'type' : 'internet-explorer-x64'}}
    for architecture, ie_info in win_search_paths.iteritems():
      if not ie_info['path']:
        continue
      if os.path.exists(os.path.join(ie_info['path'], ie_path)):
        browsers.append(
            PossibleDesktopIE(ie_info['type'], finder_options, architecture))

  return browsers
