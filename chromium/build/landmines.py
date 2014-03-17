#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This script runs every build as a hook. If it detects that the build should
be clobbered, it will touch the file <build_dir>/.landmine_triggered. The
various build scripts will then check for the presence of this file and clobber
accordingly. The script will also emit the reasons for the clobber to stdout.

A landmine is tripped when a builder checks out a different revision, and the
diff between the new landmines and the old ones is non-null. At this point, the
build is clobbered.
"""

import difflib
import gyp_helper
import logging
import optparse
import os
import sys
import subprocess
import time

import landmine_utils


SRC_DIR = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))


def get_target_build_dir(build_tool, target, is_iphone=False):
  """
  Returns output directory absolute path dependent on build and targets.
  Examples:
    r'c:\b\build\slave\win\build\src\out\Release'
    '/mnt/data/b/build/slave/linux/build/src/out/Debug'
    '/b/build/slave/ios_rel_device/build/src/xcodebuild/Release-iphoneos'

  Keep this function in sync with tools/build/scripts/slave/compile.py
  """
  ret = None
  if build_tool == 'xcode':
    ret = os.path.join(SRC_DIR, 'xcodebuild',
        target + ('-iphoneos' if is_iphone else ''))
  elif build_tool in ['make', 'ninja', 'ninja-ios']:  # TODO: Remove ninja-ios.
    ret = os.path.join(SRC_DIR, 'out', target)
  elif build_tool in ['msvs', 'vs', 'ib']:
    ret = os.path.join(SRC_DIR, 'build', target)
  else:
    raise NotImplementedError('Unexpected GYP_GENERATORS (%s)' % build_tool)
  return os.path.abspath(ret)


def set_up_landmines(target, new_landmines):
  """Does the work of setting, planting, and triggering landmines."""
  out_dir = get_target_build_dir(landmine_utils.builder(), target,
                                 landmine_utils.platform() == 'ios')

  landmines_path = os.path.join(out_dir, '.landmines')
  if not os.path.exists(out_dir):
    os.makedirs(out_dir)

  if not os.path.exists(landmines_path):
    with open(landmines_path, 'w') as f:
      f.writelines(new_landmines)
  else:
    triggered = os.path.join(out_dir, '.landmines_triggered')
    with open(landmines_path, 'r') as f:
      old_landmines = f.readlines()
    if old_landmines != new_landmines:
      old_date = time.ctime(os.stat(landmines_path).st_ctime)
      diff = difflib.unified_diff(old_landmines, new_landmines,
          fromfile='old_landmines', tofile='new_landmines',
          fromfiledate=old_date, tofiledate=time.ctime(), n=0)

      with open(triggered, 'w') as f:
        f.writelines(diff)
    elif os.path.exists(triggered):
      # Remove false triggered landmines.
      os.remove(triggered)


def process_options():
  """Returns a list of landmine emitting scripts."""
  parser = optparse.OptionParser()
  parser.add_option(
      '-s', '--landmine-scripts', action='append',
      default=[os.path.join(SRC_DIR, 'build', 'get_landmines.py')],
      help='Path to the script which emits landmines to stdout. The target '
           'is passed to this script via option -t. Note that an extra '
           'script can be specified via an env var EXTRA_LANDMINES_SCRIPT.')
  parser.add_option('-v', '--verbose', action='store_true',
      default=('LANDMINES_VERBOSE' in os.environ),
      help=('Emit some extra debugging information (default off). This option '
          'is also enabled by the presence of a LANDMINES_VERBOSE environment '
          'variable.'))

  options, args = parser.parse_args()

  if args:
    parser.error('Unknown arguments %s' % args)

  logging.basicConfig(
      level=logging.DEBUG if options.verbose else logging.ERROR)

  extra_script = os.environ.get('EXTRA_LANDMINES_SCRIPT')
  if extra_script:
    return options.landmine_scripts + [extra_script]
  else:
    return options.landmine_scripts


def main():
  landmine_scripts = process_options()
  gyp_helper.apply_chromium_gyp_env()

  for target in ('Debug', 'Release', 'Debug_x64', 'Release_x64'):
    landmines = []
    for s in landmine_scripts:
      proc = subprocess.Popen([sys.executable, s, '-t', target],
                              stdout=subprocess.PIPE)
      output, _ = proc.communicate()
      landmines.extend([('%s\n' % l.strip()) for l in output.splitlines()])
    set_up_landmines(target, landmines)

  return 0


if __name__ == '__main__':
  sys.exit(main())
