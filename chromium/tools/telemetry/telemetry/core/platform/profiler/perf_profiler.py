# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import signal
import subprocess
import sys
import tempfile

from telemetry.core import util
from telemetry.core.platform import profiler


class _SingleProcessPerfProfiler(object):
  """An internal class for using perf for a given process."""
  def __init__(self, pid, output_file, browser_backend, platform_backend):
    self._pid = pid
    self._browser_backend = browser_backend
    self._platform_backend = platform_backend
    self._output_file = output_file
    self._tmp_output_file = tempfile.NamedTemporaryFile('w', 0)
    self._is_android = platform_backend.GetOSName() == 'android'
    cmd_prefix = []
    if self._is_android:
      cmd_prefix = ['adb', '-s', browser_backend.adb.device(), 'shell',
                    '/data/local/tmp/perf']
      output_file = os.path.join('/sdcard', os.path.basename(output_file))
    else:
      cmd_prefix = ['perf']
    self._proc = subprocess.Popen(cmd_prefix +
        ['record', '--call-graph',
         '--pid', str(pid), '--output', output_file],
        stdout=self._tmp_output_file, stderr=subprocess.STDOUT)

  def CollectProfile(self):
    if ('renderer' in self._output_file and
        not self._is_android and
        not self._platform_backend.GetCommandLine(self._pid)):
      logging.warning('Renderer was swapped out during profiling. '
                      'To collect a full profile rerun with '
                      '"--extra-browser-args=--single-process"')
    if self._is_android:
      perf_pids = self._browser_backend.adb.Adb().ExtractPid('perf')
      self._browser_backend.adb.Adb().RunShellCommand(
          'kill -SIGINT ' + ' '.join(perf_pids))
    self._proc.send_signal(signal.SIGINT)
    exit_code = self._proc.wait()
    try:
      if exit_code == 128:
        raise Exception(
            """perf failed with exit code 128.
Try rerunning this script under sudo or setting
/proc/sys/kernel/perf_event_paranoid to "-1".\nOutput:\n%s""" %
            self._GetStdOut())
      elif exit_code not in (0, -2):
        raise Exception(
            'perf failed with exit code %d. Output:\n%s' % (exit_code,
                                                            self._GetStdOut()))
    finally:
      self._tmp_output_file.close()
    cmd = 'perf report'
    if self._is_android:
      self._browser_backend.adb.Adb().Adb().Pull(
          os.path.join('/sdcard', os.path.basename(self._output_file)),
          self._output_file)
      host_symfs = os.path.join(os.path.dirname(self._output_file),
                                'data', 'app-lib')
      if not os.path.exists(host_symfs):
        os.makedirs(host_symfs)
        # On Android, the --symfs parameter needs to map a directory structure
        # similar to the device, that is:
        # --symfs=/tmp/foobar and then inside foobar there'll be something like
        # /tmp/foobar/data/app-lib/$PACKAGE/libname.so
        # Assume the symbolized library under out/Release/lib is equivalent to
        # the one in the device, and symlink it in the host to match --symfs.
        device_dir = filter(
            lambda app_lib: app_lib.startswith(self._browser_backend.package),
            self._browser_backend.adb.Adb().RunShellCommand('ls /data/app-lib'))
        os.symlink(os.path.join(util.GetChromiumSrcDir(),
                                'out', 'Release', 'lib'),
                   os.path.join(host_symfs, device_dir[0]))
      print 'On Android, assuming out/Release/lib has a fresh symbolized '
      print 'library matching the one on device.'
      cmd = 'perfhost report --symfs %s' % os.path.dirname(self._output_file)
    print 'To view the profile, run:'
    print '  %s -n -i %s' % (cmd, self._output_file)
    return self._output_file

  def _GetStdOut(self):
    self._tmp_output_file.flush()
    try:
      with open(self._tmp_output_file.name) as f:
        return f.read()
    except IOError:
      return ''


class PerfProfiler(profiler.Profiler):

  def __init__(self, browser_backend, platform_backend, output_path):
    super(PerfProfiler, self).__init__(
        browser_backend, platform_backend, output_path)
    process_output_file_map = self._GetProcessOutputFileMap()
    self._process_profilers = []
    for pid, output_file in process_output_file_map.iteritems():
      if 'zygote' in output_file:
        continue
      self._process_profilers.append(
          _SingleProcessPerfProfiler(
              pid, output_file, browser_backend, platform_backend))

  @classmethod
  def name(cls):
    return 'perf'

  @classmethod
  def is_supported(cls, browser_type):
    if sys.platform != 'linux2':
      return False
    if browser_type.startswith('cros'):
      return False
    return cls._CheckLinuxPerf() or cls._CheckAndroidPerf()

  @classmethod
  def _CheckLinuxPerf(cls):
    try:
      return not subprocess.Popen(['perf', '--version'],
                                  stderr=subprocess.STDOUT,
                                  stdout=subprocess.PIPE).wait()
    except OSError:
      return False

  @classmethod
  def CustomizeBrowserOptions(cls, options):
    options.AppendExtraBrowserArgs([
        '--no-sandbox',
        '--allow-sandbox-debugging',
    ])

  @classmethod
  def _CheckAndroidPerf(cls):
    try:
      return not subprocess.Popen(['perfhost', '--version'],
                                  stderr=subprocess.STDOUT,
                                  stdout=subprocess.PIPE).wait()
    except OSError:
      return False


  def CollectProfile(self):
    output_files = []
    for single_process in self._process_profilers:
      output_files.append(single_process.CollectProfile())
    return output_files

  @classmethod
  def GetTopSamples(cls, os_name, file_name, number):
    """Parses the perf generated profile in |file_name| and returns a
    {function: period} dict of the |number| hottests functions.
    """
    assert os.path.exists(file_name)
    cmd = 'perf'
    if os_name == 'android':
      cmd = 'perfhost'
    report = subprocess.Popen(
        [cmd, 'report', '--show-total-period', '-U', '-t', '^', '-i',
         file_name],
        stdout=subprocess.PIPE, stderr=open(os.devnull, 'w')).communicate()[0]
    period_by_function = {}
    for line in report.split('\n'):
      if not line or line.startswith('#'):
        continue
      fields = line.split('^')
      if len(fields) != 5:
        continue
      period = int(fields[1])
      function = fields[4].partition(' ')[2]
      function = re.sub('<.*>', '', function)  # Strip template params.
      function = re.sub('[(].*[)]', '', function)  # Strip function params.
      period_by_function[function] = period
      if len(period_by_function) == number:
        break
    return period_by_function
