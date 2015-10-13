# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Utility script to launch browser-tests on the Chromoting bot."""
import argparse

from chromoting_test_utilities import InitialiseTestMachineForLinux
from chromoting_test_utilities import PrintHostLogContents
from chromoting_test_utilities import PROD_DIR_ID
from chromoting_test_utilities import RunCommandInSubProcess
from chromoting_test_utilities import TestCaseSetup
from chromoting_test_utilities import TestMachineCleanup

SUCCESS_INDICATOR = 'SUCCESS: all tests passed.'
TEST_FAILURE = False
FAILING_TESTS = ''
BROWSER_NOT_STARTED_ERROR = (
    'Still waiting for the following processes to finish')
TIME_OUT_INDICATOR = '(TIMED OUT)'
MAX_RETRIES = 1


def LaunchBTCommand(args, command):
  """Launches the specified browser-test command.

      If the execution failed because a browser-instance was not launched, retry
      once.
  Args:
    args: Command line args, used for test-case startup tasks.
    command: Browser-test command line.
  """
  global TEST_FAILURE, FAILING_TESTS

  retries = 0
  while retries <= MAX_RETRIES:
    TestCaseSetup(args)
    results = RunCommandInSubProcess(command)

    if SUCCESS_INDICATOR in results:
      # Test passed.
      break

    # Sometimes, during execution of browser-tests, a browser instance is
    # not started and the test times out. See http://crbug/480025.
    # To work around it, check if this execution failed owing to that
    # problem and retry.
    # There are 2 things to look for in the results:
    # A line saying "Still waiting for the following processes to finish",
    # and, because sometimes that line gets logged even if the test
    # eventually passes, we'll also look for "(TIMED OUT)", before retrying.
    if not (
        BROWSER_NOT_STARTED_ERROR in results and TIME_OUT_INDICATOR in results):
      # Test failed for some other reason. Let's not retry.
      break
    retries += 1

  # Check that the test passed.
  if SUCCESS_INDICATOR not in results:
    TEST_FAILURE = True
    # Add this command-line to list of tests that failed.
    FAILING_TESTS += command


def main(args):

  InitialiseTestMachineForLinux(args.cfg_file)

  with open(args.commands_file) as f:
    for line in f:
      # Replace the PROD_DIR value in the command-line with
      # the passed in value.
      line = line.replace(PROD_DIR_ID, args.prod_dir)
      # Launch specified command line for test.
      LaunchBTCommand(args, line)

  # All tests completed. Include host-logs in the test results.
  PrintHostLogContents()

  if TEST_FAILURE:
    print '++++++++++AT LEAST 1 TEST FAILED++++++++++'
    print FAILING_TESTS.rstrip('\n')
    print '++++++++++++++++++++++++++++++++++++++++++'
    raise Exception('At least one test failed.')

if __name__ == '__main__':

  parser = argparse.ArgumentParser()
  parser.add_argument('-f', '--commands_file',
                      help='path to file listing commands to be launched.')
  parser.add_argument('-p', '--prod_dir',
                      help='path to folder having product and test binaries.')
  parser.add_argument('-c', '--cfg_file',
                      help='path to test host config file.')
  parser.add_argument('--me2me_manifest_file',
                      help='path to me2me host manifest file.')
  parser.add_argument('--it2me_manifest_file',
                      help='path to it2me host manifest file.')
  parser.add_argument(
      '-u', '--user_profile_dir',
      help='path to user-profile-dir, used by connect-to-host tests.')
  command_line_args = parser.parse_args()
  try:
    main(command_line_args)
  finally:
    # Stop host and cleanup user-profile-dir.
    TestMachineCleanup(command_line_args.user_profile_dir)
