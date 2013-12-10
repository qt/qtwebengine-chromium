# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import subprocess
import sys
import tempfile

from telemetry.core.platform import profiler


class _SingleProcessVTuneProfiler(object):
  """An internal class for using vtune for a given process."""
  def __init__(self, pid, output_file, platform_backend):
    self._pid = pid
    self._platform_backend = platform_backend
    self._output_file = output_file
    self._tmp_output_file = tempfile.NamedTemporaryFile('w', 0)
    self._proc = subprocess.Popen(
        ['amplxe-cl', '-collect', 'hotspots',
         '-target-pid', str(pid), '-r', self._output_file],
        stdout=self._tmp_output_file, stderr=subprocess.STDOUT)

  def CollectProfile(self):
    if ('renderer' in self._output_file and
        not self._platform_backend.GetCommandLine(self._pid)):
      logging.warning('Renderer was swapped out during profiling. '
                      'To collect a full profile rerun with '
                      '"--extra-browser-args=--single-process"')
    subprocess.call(['amplxe-cl', '-command', 'stop', '-r', self._output_file])

    exit_code = self._proc.wait()
    try:
      # 1: amplxe: Error: Cannot find a running process with the specified ID.
      #    Provide a valid PID.
      if exit_code not in (0, 1):
        raise Exception(
            'amplxe-cl failed with exit code %d. Output:\n%s' % (exit_code,
            self._GetStdOut()))
    finally:
      self._tmp_output_file.close()

    if not exit_code:
      print 'To view the profile, run:'
      print '  amplxe-gui %s' % self._output_file
    return self._output_file

  def _GetStdOut(self):
    self._tmp_output_file.flush()
    try:
      with open(self._tmp_output_file.name) as f:
        return f.read()
    except IOError:
      return ''


class VTuneProfiler(profiler.Profiler):

  def __init__(self, browser_backend, platform_backend, output_path):
    super(VTuneProfiler, self).__init__(
        browser_backend, platform_backend, output_path)
    process_output_file_map = self._GetProcessOutputFileMap()
    self._process_profilers = []

    has_renderer = False
    for pid, output_file in process_output_file_map.iteritems():
      if 'renderer' in output_file:
        has_renderer = True
        break

    for pid, output_file in process_output_file_map.iteritems():
      if has_renderer:
        if not 'renderer' in output_file:
          continue
      elif not 'browser0' in output_file:
        continue

      self._process_profilers.append(
          _SingleProcessVTuneProfiler(pid, output_file, platform_backend))

  @classmethod
  def name(cls):
    return 'vtune'

  @classmethod
  def is_supported(cls, browser_type):
    if sys.platform != 'linux2':
      return False
    if (browser_type.startswith('android') or
        browser_type.startswith('cros')):
      return False
    try:
      return not subprocess.Popen(['amplxe-cl', '-version'],
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

  def CollectProfile(self):
    print 'Processing profile, this will take a few minutes...'

    output_files = []
    for single_process in self._process_profilers:
      output_files.append(single_process.CollectProfile())
    return output_files

