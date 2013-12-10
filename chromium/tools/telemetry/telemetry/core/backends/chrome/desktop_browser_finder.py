# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Finds desktop browsers that can be controlled by telemetry."""

import logging
from operator import attrgetter
import os
import platform
import subprocess
import sys

from telemetry.core import browser
from telemetry.core import platform as core_platform
from telemetry.core import possible_browser
from telemetry.core import util
from telemetry.core.backends.chrome import cros_interface
from telemetry.core.backends.chrome import desktop_browser_backend

ALL_BROWSER_TYPES = ','.join([
    'exact',
    'release',
    'release_x64',
    'debug',
    'debug_x64',
    'canary',
    'content-shell-debug',
    'content-shell-release',
    'system'])

class PossibleDesktopBrowser(possible_browser.PossibleBrowser):
  """A desktop browser that can be controlled."""

  def __init__(self, browser_type, finder_options, executable, flash_path,
               is_content_shell, browser_directory, is_local_build=False):
    super(PossibleDesktopBrowser, self).__init__(browser_type, finder_options)
    self._local_executable = executable
    self._flash_path = flash_path
    self._is_content_shell = is_content_shell
    self._browser_directory = browser_directory
    self.is_local_build = is_local_build

  def __repr__(self):
    return 'PossibleDesktopBrowser(browser_type=%s)' % self.browser_type

  def Create(self):
    backend = desktop_browser_backend.DesktopBrowserBackend(
        self.finder_options.browser_options, self._local_executable,
        self._flash_path, self._is_content_shell, self._browser_directory,
        output_profile_path=self.finder_options.output_profile_path,
        extensions_to_load=self.finder_options.extensions_to_load)
    b = browser.Browser(backend,
                        core_platform.CreatePlatformBackendForCurrentOS())
    return b

  def SupportsOptions(self, finder_options):
    if (len(finder_options.extensions_to_load) != 0) and self._is_content_shell:
      return False
    return True

  @property
  def last_modification_time(self):
    if os.path.exists(self._local_executable):
      return os.path.getmtime(self._local_executable)
    return -1

def SelectDefaultBrowser(possible_browsers):
  local_builds_by_date = [
      b for b in sorted(possible_browsers,
                        key=attrgetter('last_modification_time'))
      if b.is_local_build]
  if local_builds_by_date:
    return local_builds_by_date[-1]
  return None

def CanFindAvailableBrowsers():
  return not cros_interface.IsRunningOnCrosDevice()

def FindAllAvailableBrowsers(finder_options):
  """Finds all the desktop browsers available on this machine."""
  browsers = []

  if not CanFindAvailableBrowsers():
    return []

  has_display = True
  if (sys.platform.startswith('linux') and
      os.getenv('DISPLAY') == None):
    has_display = False

  # Look for a browser in the standard chrome build locations.
  if finder_options.chrome_root:
    chrome_root = finder_options.chrome_root
  else:
    chrome_root = util.GetChromiumSrcDir()

  if sys.platform == 'darwin':
    chromium_app_name = 'Chromium.app/Contents/MacOS/Chromium'
    content_shell_app_name = 'Content Shell.app/Contents/MacOS/Content Shell'
    mac_dir = 'mac'
    if platform.architecture()[0] == '64bit':
      mac_dir = 'mac_64'
    flash_path = os.path.join(
        chrome_root, 'third_party', 'adobe', 'flash', 'binaries', 'ppapi',
        mac_dir, 'PepperFlashPlayer.plugin')
  elif sys.platform.startswith('linux'):
    chromium_app_name = 'chrome'
    content_shell_app_name = 'content_shell'
    linux_dir = 'linux'
    if platform.architecture()[0] == '64bit':
      linux_dir = 'linux_x64'
    flash_path = os.path.join(
        chrome_root, 'third_party', 'adobe', 'flash', 'binaries', 'ppapi',
        linux_dir, 'libpepflashplayer.so')
  elif sys.platform.startswith('win'):
    chromium_app_name = 'chrome.exe'
    content_shell_app_name = 'content_shell.exe'
    win_dir = 'win'
    if platform.architecture()[0] == '64bit':
      win_dir = 'win_x64'
    flash_path = os.path.join(
        chrome_root, 'third_party', 'adobe', 'flash', 'binaries', 'ppapi',
        win_dir, 'pepflashplayer.dll')
  else:
    raise Exception('Platform not recognized')

  def IsExecutable(path):
    return os.path.isfile(path) and os.access(path, os.X_OK)

  # Add the explicit browser executable if given.
  if finder_options.browser_executable:
    normalized_executable = os.path.expanduser(
        finder_options.browser_executable)
    if IsExecutable(normalized_executable):
      browser_directory = os.path.dirname(finder_options.browser_executable)
      browsers.append(PossibleDesktopBrowser('exact', finder_options,
                                             normalized_executable, flash_path,
                                             False, browser_directory))
    else:
      logging.warning('%s specified by browser_executable does not exist',
                      normalized_executable)

  def AddIfFound(browser_type, build_dir, type_dir, app_name, content_shell):
    browser_directory = os.path.join(chrome_root, build_dir, type_dir)
    app = os.path.join(browser_directory, app_name)
    if IsExecutable(app):
      browsers.append(PossibleDesktopBrowser(browser_type, finder_options,
                                             app, flash_path, content_shell,
                                             browser_directory,
                                             is_local_build=True))
      return True
    return False

  # Add local builds
  for build_dir, build_type in util.GetBuildDirectories():
    AddIfFound(build_type.lower(), build_dir, build_type,
               chromium_app_name, False)
    AddIfFound('content-shell-' + build_type.lower(), build_dir, build_type,
               content_shell_app_name, True)

  # Mac-specific options.
  if sys.platform == 'darwin':
    mac_canary_root = '/Applications/Google Chrome Canary.app/'
    mac_canary = mac_canary_root + 'Contents/MacOS/Google Chrome Canary'
    mac_system_root = '/Applications/Google Chrome.app'
    mac_system = mac_system_root + '/Contents/MacOS/Google Chrome'
    if IsExecutable(mac_canary):
      browsers.append(PossibleDesktopBrowser('canary', finder_options,
                                             mac_canary, None, False,
                                             mac_canary_root))

    if IsExecutable(mac_system):
      browsers.append(PossibleDesktopBrowser('system', finder_options,
                                             mac_system, None, False,
                                             mac_system_root))

  # Linux specific options.
  if sys.platform.startswith('linux'):
    # Look for a google-chrome instance.
    found = False
    try:
      with open(os.devnull, 'w') as devnull:
        found = subprocess.call(['google-chrome', '--version'],
                                stdout=devnull, stderr=devnull) == 0
    except OSError:
      pass
    if found:
      browsers.append(PossibleDesktopBrowser('system', finder_options,
                                             'google-chrome', None, False,
                                             '/opt/google/chrome'))

  # Win32-specific options.
  if sys.platform.startswith('win'):
    system_path = os.path.join('Google', 'Chrome', 'Application')
    canary_path = os.path.join('Google', 'Chrome SxS', 'Application')

    win_search_paths = [os.getenv('PROGRAMFILES(X86)'),
                        os.getenv('PROGRAMFILES'),
                        os.getenv('LOCALAPPDATA')]

    def AddIfFoundWin(browser_name, app_path):
      browser_directory = os.path.join(path, app_path)
      app = os.path.join(browser_directory, chromium_app_name)
      if IsExecutable(app):
        browsers.append(PossibleDesktopBrowser(browser_name, finder_options,
                                               app, flash_path, False,
                                               browser_directory))
        return True
      return False

    for path in win_search_paths:
      if not path:
        continue
      if AddIfFoundWin('canary', canary_path):
        break

    for path in win_search_paths:
      if not path:
        continue
      if AddIfFoundWin('system', system_path):
        break

  if len(browsers) and not has_display:
    logging.warning(
      'Found (%s), but you do not have a DISPLAY environment set.' %
      ','.join([b.browser_type for b in browsers]))
    return []

  return browsers
