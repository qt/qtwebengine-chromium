#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run Manual Test Bisect Tool

An example usage:
tools/run-bisect-manual-test.py -g 201281 -b 201290

On Linux platform, follow the instructions in this document
https://code.google.com/p/chromium/wiki/LinuxSUIDSandboxDevelopment
to setup the sandbox manually before running the script. Otherwise the script
fails to launch Chrome and exits with an error.

"""

import os
import subprocess
import sys

CROS_BOARD_ENV = 'BISECT_CROS_BOARD'
CROS_IP_ENV = 'BISECT_CROS_IP'
_DIR_TOOLS_ROOT = os.path.abspath(os.path.dirname(__file__))

sys.path.append(os.path.join(_DIR_TOOLS_ROOT, 'telemetry'))
from telemetry.core import browser_options


def _RunBisectionScript(options):
  """Attempts to execute src/tools/bisect-perf-regression.py with the parameters
  passed in.

  Args:
    options: The configuration options to pass to the bisect script.

  Returns:
    0 on success, otherwise 1.
  """
  test_command = 'python %s --browser=%s --chrome-root=.' %\
      (os.path.join(_DIR_TOOLS_ROOT, 'bisect-manual-test.py'),
       options.browser_type)

  cmd = ['python', os.path.join(_DIR_TOOLS_ROOT, 'bisect-perf-regression.py'),
         '-c', test_command,
         '-g', options.good_revision,
         '-b', options.bad_revision,
         '-m', 'manual_test/manual_test',
         '-r', '1',
         '--working_directory', options.working_directory,
         '--build_preference', 'ninja',
         '--use_goma',
         '--no_custom_deps']

  if 'cros' in options.browser_type:
    cmd.extend(['--target_platform', 'cros'])

    if os.environ[CROS_BOARD_ENV] and os.environ[CROS_IP_ENV]:
      cmd.extend(['--cros_board', os.environ[CROS_BOARD_ENV]])
      cmd.extend(['--cros_remote_ip', os.environ[CROS_IP_ENV]])
    else:
      print 'Error: Cros build selected, but BISECT_CROS_IP or'\
            'BISECT_CROS_BOARD undefined.'
      print
      return 1
  elif 'android' in options.browser_type:
    cmd.extend(['--target_platform', 'android'])

  cmd = [str(c) for c in cmd]

  return_code = subprocess.call(cmd)

  if return_code:
    print 'Error: bisect-perf-regression.py returned with error %d' %\
        return_code
    print

  return return_code


def main():
  usage = ('%prog [options]\n'
           'Used to run the bisection script with a manual test.')

  options = browser_options.BrowserFinderOptions('release')
  parser = options.CreateParser(usage)

  parser.add_option('-b', '--bad_revision',
                    type='str',
                    help='A bad revision to start bisection. ' +
                    'Must be later than good revision. May be either a git' +
                    ' or svn revision.')
  parser.add_option('-g', '--good_revision',
                    type='str',
                    help='A revision to start bisection where performance' +
                    ' test is known to pass. Must be earlier than the ' +
                    'bad revision. May be either a git or svn revision.')
  parser.add_option('-w', '--working_directory',
                    type='str',
                    default='..',
                    help='A working directory to supply to the bisection '
                    'script, which will use it as the location to checkout '
                    'a copy of the chromium depot.')

  options, args = parser.parse_args()
  error_msg = ''
  if not options.good_revision:
    error_msg += 'Error: missing required parameter: --good_revision\n'
  if not options.bad_revision:
    error_msg += 'Error: missing required parameter: --bad_revision\n'

  if error_msg:
    print error_msg
    parser.print_help()
    return 1

  if sys.platform.startswith('linux'):
    if not os.environ.get('CHROME_DEVEL_SANDBOX'):
      print 'SUID sandbox has not been setup.'\
            ' See https://code.google.com/p/chromium/wiki/'\
            'LinuxSUIDSandboxDevelopment for more information.'
      return 1

  return _RunBisectionScript(options)


if __name__ == '__main__':
  sys.exit(main())
