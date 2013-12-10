# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess
import sys
import time

from telemetry.core import exceptions
from telemetry.core import util
from telemetry.core.backends import adb_commands
from telemetry.core.backends import android_rndis
from telemetry.core.backends import browser_backend
from telemetry.core.backends.chrome import chrome_browser_backend


class AndroidBrowserBackendSettings(object):
  def __init__(self, adb, activity, cmdline_file, package, pseudo_exec_name):
    self.adb = adb
    self.activity = activity
    self.cmdline_file = cmdline_file
    self.package = package
    self.pseudo_exec_name = pseudo_exec_name

  def GetDevtoolsRemotePort(self):
    raise NotImplementedError()

  def RemoveProfile(self):
    self.adb.RunShellCommand(
        'su -c rm -r "%s"' % self.profile_dir)

  def PushProfile(self, _):
    logging.critical('Profiles cannot be overriden with current configuration')
    sys.exit(1)

  @property
  def is_content_shell(self):
    return False

  @property
  def profile_dir(self):
    raise NotImplementedError()


class ChromeBackendSettings(AndroidBrowserBackendSettings):
  # Stores a default Preferences file, re-used to speed up "--page-repeat".
  _default_preferences_file = None

  def __init__(self, adb, package):
    super(ChromeBackendSettings, self).__init__(
        adb=adb,
        activity='com.google.android.apps.chrome.Main',
        cmdline_file='/data/local/chrome-command-line',
        package=package,
        pseudo_exec_name='chrome')

  def GetDevtoolsRemotePort(self):
    return 'localabstract:chrome_devtools_remote'

  def PushProfile(self, new_profile_dir):
    self.adb.Push(new_profile_dir, self.profile_dir)

  @property
  def profile_dir(self):
    return '/data/data/%s/app_chrome/' % self.package


class ContentShellBackendSettings(AndroidBrowserBackendSettings):
  def __init__(self, adb, package):
    super(ContentShellBackendSettings, self).__init__(
        adb=adb,
        activity='org.chromium.content_shell_apk.ContentShellActivity',
        cmdline_file='/data/local/tmp/content-shell-command-line',
        package=package,
        pseudo_exec_name='content_shell')

  def GetDevtoolsRemotePort(self):
    return 'localabstract:content_shell_devtools_remote'

  @property
  def is_content_shell(self):
    return True

  @property
  def profile_dir(self):
    return '/data/data/%s/app_content_shell/' % self.package


class ChromiumTestShellBackendSettings(AndroidBrowserBackendSettings):
  def __init__(self, adb, package):
    super(ChromiumTestShellBackendSettings, self).__init__(
          adb=adb,
          activity='org.chromium.chrome.testshell.ChromiumTestShellActivity',
          cmdline_file='/data/local/tmp/chromium-testshell-command-line',
          package=package,
          pseudo_exec_name='chromium_testshell')

  def GetDevtoolsRemotePort(self):
    return 'localabstract:chromium_testshell_devtools_remote'

  @property
  def is_content_shell(self):
    return True

  @property
  def profile_dir(self):
    return '/data/data/%s/app_chromiumtestshell/' % self.package


class WebviewBackendSettings(AndroidBrowserBackendSettings):
  def __init__(self, adb, package):
    super(WebviewBackendSettings, self).__init__(
        adb=adb,
        activity='com.android.webview.chromium.shell.TelemetryActivity',
        cmdline_file='/data/local/tmp/webview-command-line',
        package=package,
        pseudo_exec_name='webview')

  def GetDevtoolsRemotePort(self):
    # The DevTools socket name for WebView depends on the activity PID's.
    retries = 0
    timeout = 1
    pid = None
    while True:
      pids = self.adb.ExtractPid(self.package)
      if (len(pids) > 0):
        pid = pids[-1]
        break
      time.sleep(timeout)
      retries += 1
      timeout *= 2
      if retries == 4:
        logging.critical('android_browser_backend: Timeout while waiting for '
                         'activity %s:%s to come up',
                         self.package,
                         self.activity)
        raise exceptions.BrowserGoneException('Timeout waiting for PID.')
    return 'localabstract:webview_devtools_remote_%s' % str(pid)

  @property
  def profile_dir(self):
    return '/data/data/%s/app_webview/' % self.package


class AndroidBrowserBackend(chrome_browser_backend.ChromeBrowserBackend):
  """The backend for controlling a browser instance running on Android.
  """
  def __init__(self, browser_options, backend_settings, rndis,
               output_profile_path, extensions_to_load):
    super(AndroidBrowserBackend, self).__init__(
        is_content_shell=backend_settings.is_content_shell,
        supports_extensions=False, browser_options=browser_options,
        output_profile_path=output_profile_path,
        extensions_to_load=extensions_to_load)
    if len(extensions_to_load) > 0:
      raise browser_backend.ExtensionsNotSupportedException(
          'Android browser does not support extensions.')

    from telemetry.core.backends.chrome import chrome_browser_options
    assert isinstance(browser_options,
                      chrome_browser_options.AndroidBrowserOptions)

    # Initialize fields so that an explosion during init doesn't break in Close.
    self._adb = backend_settings.adb
    self._backend_settings = backend_settings
    self._saved_cmdline = None
    if not self.browser_options.keep_test_server_ports:
      adb_commands.ResetTestServerPortAllocation()
    self._port = adb_commands.AllocateTestServerPort()

    # Kill old browser.
    self._adb.CloseApplication(self._backend_settings.package)

    if self._adb.Adb().CanAccessProtectedFileContents():
      if not self.browser_options.dont_override_profile:
        self._backend_settings.RemoveProfile()
      if self.browser_options.profile_dir:
        self._backend_settings.PushProfile(self.browser_options.profile_dir)

    # Pre-configure RNDIS forwarding.
    self._rndis_forwarder = None
    if rndis:
      self._rndis_forwarder = android_rndis.RndisForwarderWithRoot(self._adb)
      self.WEBPAGEREPLAY_HOST = self._rndis_forwarder.host_ip
    # TODO(szym): only override DNS if WPR has privileges to proxy on port 25.
    self._override_dns = False

    self._SetUpCommandLine()

  def _SetUpCommandLine(self):
    def QuoteIfNeeded(arg):
      # Escape 'key=valueA valueB' to 'key="valueA valueB"'
      # Already quoted values, or values without space are left untouched.
      # This is required so CommandLine.java can parse valueB correctly rather
      # than as a separate switch.
      params = arg.split('=')
      if len(params) != 2:
        return arg
      key, values = params
      if ' ' not in values:
        return arg
      if values[0] in '"\'' and values[-1] == values[0]:
        return arg
      return '%s="%s"' % (key, values)

    args = [self._backend_settings.pseudo_exec_name]
    args.extend(self.GetBrowserStartupArgs())
    args = ' '.join(map(QuoteIfNeeded, args))

    self._SetCommandLineFile(args)

  def _SetCommandLineFile(self, file_contents):
    def IsProtectedFile(name):
      if self._adb.Adb().FileExistsOnDevice(name):
        return not self._adb.Adb().IsFileWritableOnDevice(name)
      else:
        parent_name = os.path.dirname(name)
        if parent_name != '':
          return IsProtectedFile(parent_name)
        else:
          return True

    if IsProtectedFile(self._backend_settings.cmdline_file):
      if not self._adb.Adb().CanAccessProtectedFileContents():
        logging.critical('Cannot set Chrome command line. '
                         'Fix this by flashing to a userdebug build.')
        sys.exit(1)
      self._saved_cmdline = ''.join(self._adb.Adb().GetProtectedFileContents(
          self._backend_settings.cmdline_file) or [])
      self._adb.Adb().SetProtectedFileContents(
          self._backend_settings.cmdline_file, file_contents)
    else:
      self._saved_cmdline = ''.join(self._adb.Adb().GetFileContents(
          self._backend_settings.cmdline_file) or [])
      self._adb.Adb().SetFileContents(self._backend_settings.cmdline_file,
                                      file_contents)

  def Start(self):
    self._adb.RunShellCommand('logcat -c')
    self._adb.StartActivity(self._backend_settings.package,
                            self._backend_settings.activity,
                            True,
                            None,
                            None,
                            'about:blank')

    self._adb.Forward('tcp:%d' % self._port,
                      self._backend_settings.GetDevtoolsRemotePort())

    try:
      self._WaitForBrowserToComeUp()
      self._PostBrowserStartupInitialization()
    except exceptions.BrowserGoneException:
      logging.critical('Failed to connect to browser.')
      if not self._adb.Adb().CanAccessProtectedFileContents():
        logging.critical(
          'Resolve this by either: '
          '(1) Flashing to a userdebug build OR '
          '(2) Manually enabling web debugging in Chrome at '
          'Settings > Developer tools > Enable USB Web debugging.')
      sys.exit(1)
    except:
      import traceback
      traceback.print_exc()
      self.Close()
      raise

  def GetBrowserStartupArgs(self):
    args = super(AndroidBrowserBackend, self).GetBrowserStartupArgs()
    if self._override_dns:
      args = [arg for arg in args
              if not arg.startswith('--host-resolver-rules')]
    args.append('--enable-remote-debugging')
    args.append('--no-restore-state')
    args.append('--disable-fre')
    return args

  @property
  def adb(self):
    return self._adb

  @property
  def pid(self):
    return int(self._adb.ExtractPid(self._backend_settings.package)[0])

  @property
  def browser_directory(self):
    return None

  @property
  def profile_directory(self):
    return self._backend_settings.profile_dir

  @property
  def package(self):
    return self._backend_settings.package

  def __del__(self):
    self.Close()

  def Close(self):
    super(AndroidBrowserBackend, self).Close()
    self._SetCommandLineFile(self._saved_cmdline or '')
    self._adb.CloseApplication(self._backend_settings.package)

    if self._output_profile_path:
      logging.info("Pulling profile directory from device: '%s'->'%s'.",
                   self._backend_settings.profile_dir,
                   self._output_profile_path)
      # To minimize bandwidth it might be good to look at whether all the data
      # pulled down is really needed e.g. .pak files.
      self._adb.Pull(self._backend_settings.profile_dir,
                     self._output_profile_path)

  def IsBrowserRunning(self):
    pids = self._adb.ExtractPid(self._backend_settings.package)
    return len(pids) != 0

  def GetRemotePort(self, local_port):
    return local_port

  def GetStandardOutput(self):
    return '\n'.join(self._adb.RunShellCommand('logcat -d -t 500'))

  def GetStackTrace(self):
    def Decorate(title, content):
      return title + '\n' + content + '\n' + '*' * 80 + '\n'
    # Get the last lines of logcat (large enough to contain stacktrace)
    logcat = self.GetStandardOutput()
    ret = Decorate('Logcat', logcat)
    stack = os.path.join(util.GetChromiumSrcDir(), 'third_party',
                         'android_platform', 'development', 'scripts', 'stack')
    # Try to symbolize logcat.
    if os.path.exists(stack):
      p = subprocess.Popen([stack], stdin=subprocess.PIPE,
                           stdout=subprocess.PIPE)
      ret += Decorate('Stack from Logcat', p.communicate(input=logcat)[0])

    # Try to get tombstones.
    tombstones = os.path.join(util.GetChromiumSrcDir(), 'build', 'android',
                              'tombstones.py')
    if os.path.exists(tombstones):
      ret += Decorate('Tombstones',
                      subprocess.Popen([tombstones, '-w', '--device',
                                        self._adb.device()],
                                       stdout=subprocess.PIPE).communicate()[0])
    return ret

  def AddReplayServerOptions(self, extra_wpr_args):
    """Override. Only add --no-dns_forwarding if not using RNDIS."""
    if not self._override_dns:
      extra_wpr_args.append('--no-dns_forwarding')

  def CreateForwarder(self, *port_pairs):
    if self._rndis_forwarder:
      forwarder = self._rndis_forwarder
      forwarder.SetPorts(*port_pairs)
      assert self.WEBPAGEREPLAY_HOST == forwarder.host_ip, (
        'Host IP address on the RNDIS interface changed. Must restart browser!')
      if self._override_dns:
        forwarder.OverrideDns()
      return forwarder
    assert not self._override_dns, ('The user-space forwarder does not support '
                                    'DNS override!')
    logging.warning('Using the user-space forwarder.\n')
    return adb_commands.Forwarder(self._adb, *port_pairs)
