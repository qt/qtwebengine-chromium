#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Defer to run_test_cases.py."""

import os
import optparse
import sys

ROOT_DIR = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def pop_known_arguments(args):
  """Extracts known arguments from the args if present."""
  rest = []
  run_test_cases_extra_args = []
  for arg in args:
    if arg.startswith(('--gtest_filter=', '--gtest_output=', '--clusters=')):
      run_test_cases_extra_args.append(arg)
    elif arg in ('--run-manual', '--verbose'):
      run_test_cases_extra_args.append(arg)
    elif arg == '--gtest_print_time':
      # Ignore.
      pass
    elif 'interactive_ui_tests' in arg:
      # Run this test in a single thread. It is useful to run it under
      # run_test_cases so automatic flaky test workaround is still used.
      run_test_cases_extra_args.append('-j1')
      rest.append(arg)
    elif 'browser_tests' in arg:
      # Test cases in this executable fire up *a lot* of child processes,
      # causing huge memory bottleneck. So use less than N-cpus jobs.
      run_test_cases_extra_args.append('--use-less-jobs')
      rest.append(arg)
    else:
      rest.append(arg)

  # Use --jobs arg if exist.
  for arg in args:
    if arg.startswith('--jobs='):
      run_test_cases_extra_args.append(arg)
      break

  return run_test_cases_extra_args, rest


def main():
  parser = optparse.OptionParser()

  group = optparse.OptionGroup(
      parser, 'Compability flag with the old sharding_supervisor')
  group.add_option(
      '--no-color', action='store_true', help='Ignored')
  group.add_option(
      '--retry-failed', action='store_true', help='Ignored')
  group.add_option(
      '-t', '--timeout', type='int', help='Kept as --timeout')
  group.add_option(
      '--total-slaves', type='int', default=1, help='Converted to --index')
  group.add_option(
      '--slave-index', type='int', default=0, help='Converted to --shards')
  parser.add_option_group(group)
  group = optparse.OptionGroup(
      parser, 'Options of run_test_cases.py passed through')
  group.add_option(
      '--retries', type='int', help='Kept as --retries')
  group.add_option(
      '--verbose', action='count', default=0, help='Kept as --verbose')
  parser.add_option_group(group)

  parser.disable_interspersed_args()
  options, args = parser.parse_args()

  swarm_client_dir = os.path.join(
      ROOT_DIR, 'tools', 'swarm_client', 'googletest')
  sys.path.insert(0, swarm_client_dir)

  cmd = [
    '--shards', str(options.total_slaves),
    '--index', str(options.slave_index),
    '--no-dump',
    '--no-cr',
  ]
  if options.timeout is not None:
    cmd.extend(['--timeout', str(options.timeout)])
  if options.retries is not None:
    cmd.extend(['--retries', str(options.retries)])
  if options.verbose is not None:
    cmd.extend(['--verbose'] * options.verbose)

  run_test_cases_extra_args, rest = pop_known_arguments(args)

  import run_test_cases  # pylint: disable=F0401

  return run_test_cases.main(cmd + run_test_cases_extra_args + ['--'] + rest)


if __name__ == '__main__':
  sys.exit(main())
