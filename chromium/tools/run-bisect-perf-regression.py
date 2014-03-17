#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run Performance Test Bisect Tool

This script is used by a trybot to run the src/tools/bisect-perf-regression.py
script with the parameters specified in run-bisect-perf-regression.cfg. It will
check out a copy of the depot in a subdirectory 'bisect' of the working
directory provided, and run the bisect-perf-regression.py script there.

"""

import imp
import optparse
import os
import subprocess
import sys
import traceback

import bisect_utils
bisect = imp.load_source('bisect-perf-regression',
    os.path.join(os.path.abspath(os.path.dirname(sys.argv[0])),
        'bisect-perf-regression.py'))


CROS_BOARD_ENV = 'BISECT_CROS_BOARD'
CROS_IP_ENV = 'BISECT_CROS_IP'


class Goma(object):

  def __init__(self, path_to_goma):
    self._abs_path_to_goma = None
    self._abs_path_to_goma_file = None
    if path_to_goma:
      self._abs_path_to_goma = os.path.abspath(path_to_goma)
      self._abs_path_to_goma_file = self._GetExecutablePath(
          self._abs_path_to_goma)

  def __enter__(self):
    if self._HasGOMAPath():
      self._SetupAndStart()
    return self

  def __exit__(self, *_):
    if self._HasGOMAPath():
      self._Stop()

  def _HasGOMAPath(self):
    return bool(self._abs_path_to_goma)

  def _GetExecutablePath(self, path_to_goma):
    if os.name == 'nt':
      return os.path.join(path_to_goma, 'goma_ctl.bat')
    else:
      return os.path.join(path_to_goma, 'goma_ctl.sh')

  def _SetupEnvVars(self):
    if os.name == 'nt':
      os.environ['CC'] = (os.path.join(self._abs_path_to_goma, 'gomacc.exe') +
          ' cl.exe')
      os.environ['CXX'] = (os.path.join(self._abs_path_to_goma, 'gomacc.exe') +
          ' cl.exe')
    else:
      os.environ['PATH'] = os.pathsep.join([self._abs_path_to_goma,
          os.environ['PATH']])

  def _SetupAndStart(self):
    """Sets up GOMA and launches it.

    Args:
      path_to_goma: Path to goma directory.

    Returns:
      True if successful."""
    self._SetupEnvVars()

    # Sometimes goma is lingering around if something went bad on a previous
    # run. Stop it before starting a new process. Can ignore the return code
    # since it will return an error if it wasn't running.
    self._Stop()

    if subprocess.call([self._abs_path_to_goma_file, 'start']):
      raise RuntimeError('GOMA failed to start.')

  def _Stop(self):
    subprocess.call([self._abs_path_to_goma_file, 'stop'])



def _LoadConfigFile(path_to_file):
  """Attempts to load the specified config file as a module
  and grab the global config dict.

  Args:
    path_to_file: Path to the file.

  Returns:
    The config dict which should be formatted as follows:
    {'command': string, 'good_revision': string, 'bad_revision': string
     'metric': string, etc...}.
    Returns None on failure.
  """
  try:
    local_vars = {}
    execfile(path_to_file, local_vars)

    return local_vars['config']
  except:
    print
    traceback.print_exc()
    print
    return {}


def _OutputFailedResults(text_to_print):
  bisect_utils.OutputAnnotationStepStart('Results - Failed')
  print
  print text_to_print
  print
  bisect_utils.OutputAnnotationStepClosed()


def _CreateBisectOptionsFromConfig(config):
  opts_dict = {}
  opts_dict['command'] = config['command']
  opts_dict['metric'] = config['metric']

  if config['repeat_count']:
    opts_dict['repeat_test_count'] = int(config['repeat_count'])

  if config['truncate_percent']:
    opts_dict['truncate_percent'] = int(config['truncate_percent'])

  if config['max_time_minutes']:
    opts_dict['max_time_minutes'] = int(config['max_time_minutes'])

  if config.has_key('use_goma'):
    opts_dict['use_goma'] = config['use_goma']

  opts_dict['build_preference'] = 'ninja'
  opts_dict['output_buildbot_annotations'] = True

  if '--browser=cros' in config['command']:
    opts_dict['target_platform'] = 'cros'

    if os.environ[CROS_BOARD_ENV] and os.environ[CROS_IP_ENV]:
      opts_dict['cros_board'] = os.environ[CROS_BOARD_ENV]
      opts_dict['cros_remote_ip'] = os.environ[CROS_IP_ENV]
    else:
      raise RuntimeError('Cros build selected, but BISECT_CROS_IP or'
          'BISECT_CROS_BOARD undefined.')
  elif 'android' in config['command']:
    if 'android-chrome' in config['command']:
      opts_dict['target_platform'] = 'android-chrome'
    else:
      opts_dict['target_platform'] = 'android'

  return bisect.BisectOptions.FromDict(opts_dict)


def _RunPerformanceTest(config, path_to_file):
  # Bisect script expects to be run from src
  os.chdir(os.path.join(path_to_file, '..'))

  bisect_utils.OutputAnnotationStepStart('Building With Patch')

  opts = _CreateBisectOptionsFromConfig(config)
  b = bisect.BisectPerformanceMetrics(None, opts)

  if bisect_utils.RunGClient(['runhooks']):
    raise RuntimeError('Failed to run gclient runhooks')

  if not b.BuildCurrentRevision('chromium'):
    raise RuntimeError('Patched version failed to build.')

  bisect_utils.OutputAnnotationStepClosed()
  bisect_utils.OutputAnnotationStepStart('Running With Patch')

  results_with_patch = b.RunPerformanceTestAndParseResults(
      opts.command, opts.metric, reset_on_first_run=True, results_label='Patch')

  if results_with_patch[1]:
    raise RuntimeError('Patched version failed to run performance test.')

  bisect_utils.OutputAnnotationStepClosed()

  bisect_utils.OutputAnnotationStepStart('Reverting Patch')
  if bisect_utils.RunGClient(['revert']):
    raise RuntimeError('Failed to run gclient runhooks')
  bisect_utils.OutputAnnotationStepClosed()

  bisect_utils.OutputAnnotationStepStart('Building Without Patch')

  if bisect_utils.RunGClient(['runhooks']):
    raise RuntimeError('Failed to run gclient runhooks')

  if not b.BuildCurrentRevision('chromium'):
    raise RuntimeError('Unpatched version failed to build.')

  bisect_utils.OutputAnnotationStepClosed()
  bisect_utils.OutputAnnotationStepStart('Running Without Patch')

  results_without_patch = b.RunPerformanceTestAndParseResults(
      opts.command, opts.metric, upload_on_last_run=True, results_label='ToT')

  if results_without_patch[1]:
    raise RuntimeError('Unpatched version failed to run performance test.')

  # Find the link to the cloud stored results file.
  output = results_without_patch[2]
  cloud_file_link = [t for t in output.splitlines()
      if 'storage.googleapis.com/chromium-telemetry/html-results/' in t]
  if cloud_file_link:
    # What we're getting here is basically "View online at http://..." so parse
    # out just the url portion.
    cloud_file_link = cloud_file_link[0]
    cloud_file_link = [t for t in cloud_file_link.split(' ')
        if 'storage.googleapis.com/chromium-telemetry/html-results/' in t]
    assert cloud_file_link, "Couldn't parse url from output."
    cloud_file_link = cloud_file_link[0]
  else:
    cloud_file_link = ''

  # Calculate the % difference in the means of the 2 runs.
  percent_diff_in_means = (results_with_patch[0]['mean'] /
      max(0.0001, results_without_patch[0]['mean'])) * 100.0 - 100.0
  std_err = bisect.CalculatePooledStandardError(
      [results_with_patch[0]['values'], results_without_patch[0]['values']])

  bisect_utils.OutputAnnotationStepClosed()
  bisect_utils.OutputAnnotationStepStart('Results - %.02f +- %0.02f delta' %
      (percent_diff_in_means, std_err))
  print ' %s %s %s' % (''.center(10, ' '), 'Mean'.center(20, ' '),
      'Std. Error'.center(20, ' '))
  print ' %s %s %s' % ('Patch'.center(10, ' '),
      ('%.02f' % results_with_patch[0]['mean']).center(20, ' '),
      ('%.02f' % results_with_patch[0]['std_err']).center(20, ' '))
  print ' %s %s %s' % ('No Patch'.center(10, ' '),
      ('%.02f' % results_without_patch[0]['mean']).center(20, ' '),
      ('%.02f' % results_without_patch[0]['std_err']).center(20, ' '))
  if cloud_file_link:
    bisect_utils.OutputAnnotationStepLink('HTML Results', cloud_file_link)
  bisect_utils.OutputAnnotationStepClosed()


def _SetupAndRunPerformanceTest(config, path_to_file, path_to_goma):
  """Attempts to build and run the current revision with and without the
  current patch, with the parameters passed in.

  Args:
    config: The config read from run-perf-test.cfg.
    path_to_file: Path to the bisect-perf-regression.py script.
    path_to_goma: Path to goma directory.

  Returns:
    0 on success, otherwise 1.
  """
  try:
    with Goma(path_to_goma) as goma:
      config['use_goma'] = bool(path_to_goma)
      _RunPerformanceTest(config, path_to_file)
    return 0
  except RuntimeError, e:
    bisect_utils.OutputAnnotationStepClosed()
    _OutputFailedResults('Error: %s' % e.message)
    return 1


def _RunBisectionScript(config, working_directory, path_to_file, path_to_goma,
    path_to_extra_src, dry_run):
  """Attempts to execute src/tools/bisect-perf-regression.py with the parameters
  passed in.

  Args:
    config: A dict containing the parameters to pass to the script.
    working_directory: A working directory to provide to the
      bisect-perf-regression.py script, where it will store it's own copy of
      the depot.
    path_to_file: Path to the bisect-perf-regression.py script.
    path_to_goma: Path to goma directory.
    path_to_extra_src: Path to extra source file.
    dry_run: Do a dry run, skipping sync, build, and performance testing steps.

  Returns:
    0 on success, otherwise 1.
  """
  bisect_utils.OutputAnnotationStepStart('Config')
  print
  for k, v in config.iteritems():
    print '  %s : %s' % (k, v)
  print
  bisect_utils.OutputAnnotationStepClosed()

  cmd = ['python', os.path.join(path_to_file, 'bisect-perf-regression.py'),
         '-c', config['command'],
         '-g', config['good_revision'],
         '-b', config['bad_revision'],
         '-m', config['metric'],
         '--working_directory', working_directory,
         '--output_buildbot_annotations']

  if config['repeat_count']:
    cmd.extend(['-r', config['repeat_count']])

  if config['truncate_percent']:
    cmd.extend(['-t', config['truncate_percent']])

  if config['max_time_minutes']:
    cmd.extend(['--max_time_minutes', config['max_time_minutes']])

  cmd.extend(['--build_preference', 'ninja'])

  if '--browser=cros' in config['command']:
    cmd.extend(['--target_platform', 'cros'])

    if os.environ[CROS_BOARD_ENV] and os.environ[CROS_IP_ENV]:
      cmd.extend(['--cros_board', os.environ[CROS_BOARD_ENV]])
      cmd.extend(['--cros_remote_ip', os.environ[CROS_IP_ENV]])
    else:
      print 'Error: Cros build selected, but BISECT_CROS_IP or'\
            'BISECT_CROS_BOARD undefined.'
      print
      return 1

  if 'android' in config['command']:
    if 'android-chrome' in config['command']:
      cmd.extend(['--target_platform', 'android-chrome'])
    else:
      cmd.extend(['--target_platform', 'android'])

  if path_to_goma:
    cmd.append('--use_goma')

  if path_to_extra_src:
    cmd.extend(['--extra_src', path_to_extra_src])

  if dry_run:
    cmd.extend(['--debug_ignore_build', '--debug_ignore_sync',
        '--debug_ignore_perf_test'])
  cmd = [str(c) for c in cmd]

  with Goma(path_to_goma) as goma:
    return_code = subprocess.call(cmd)

  if return_code:
    print 'Error: bisect-perf-regression.py returned with error %d' %\
        return_code
    print

  return return_code


def main():

  usage = ('%prog [options] [-- chromium-options]\n'
           'Used by a trybot to run the bisection script using the parameters'
           ' provided in the run-bisect-perf-regression.cfg file.')

  parser = optparse.OptionParser(usage=usage)
  parser.add_option('-w', '--working_directory',
                    type='str',
                    help='A working directory to supply to the bisection '
                    'script, which will use it as the location to checkout '
                    'a copy of the chromium depot.')
  parser.add_option('-p', '--path_to_goma',
                    type='str',
                    help='Path to goma directory. If this is supplied, goma '
                    'builds will be enabled.')
  parser.add_option('--extra_src',
                    type='str',
                    help='Path to extra source file. If this is supplied, '
                    'bisect script will use this to override default behavior.')
  parser.add_option('--dry_run',
                    action="store_true",
                    help='The script will perform the full bisect, but '
                    'without syncing, building, or running the performance '
                    'tests.')
  (opts, args) = parser.parse_args()

  path_to_current_directory = os.path.abspath(os.path.dirname(sys.argv[0]))
  path_to_bisect_cfg = os.path.join(path_to_current_directory,
      'run-bisect-perf-regression.cfg')

  config = _LoadConfigFile(path_to_bisect_cfg)

  # Check if the config is empty
  config_has_values = [v for v in config.values() if v]

  if config and config_has_values:
    if not opts.working_directory:
      print 'Error: missing required parameter: --working_directory'
      print
      parser.print_help()
      return 1

    return _RunBisectionScript(config, opts.working_directory,
        path_to_current_directory, opts.path_to_goma, opts.extra_src,
        opts.dry_run)
  else:
    perf_cfg_files = ['run-perf-test.cfg', os.path.join('..', 'third_party',
        'WebKit', 'Tools', 'run-perf-test.cfg')]

    for current_perf_cfg_file in perf_cfg_files:
      path_to_perf_cfg = os.path.join(
          os.path.abspath(os.path.dirname(sys.argv[0])), current_perf_cfg_file)

      config = _LoadConfigFile(path_to_perf_cfg)
      config_has_values = [v for v in config.values() if v]

      if config and config_has_values:
        return _SetupAndRunPerformanceTest(config, path_to_current_directory,
            opts.path_to_goma)

    print 'Error: Could not load config file. Double check your changes to '\
          'run-bisect-perf-regression.cfg/run-perf-test.cfg for syntax errors.'
    print
    return 1


if __name__ == '__main__':
  sys.exit(main())
