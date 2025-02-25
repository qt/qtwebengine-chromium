#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib
import platform
import re

USE_PYTHON3 = True


def ModifiedFiles(input_api, filename_pattern="*.py"):
  files = [file.AbsoluteLocalPath() for file in input_api.AffectedFiles()]
  files_to_check = []
  for file_path in files:
    if not input_api.fnmatch.fnmatch(file_path, filename_pattern):
      continue
    file_path_pattern = re.escape(
        input_api.os_path.relpath(file_path, input_api.PresubmitLocalPath()))
    files_to_check.append(file_path_pattern)
  return files_to_check


def CheckChange(input_api, output_api, on_commit):
  tests = []
  results = []
  testing_env = dict(input_api.environ)
  testing_path = pathlib.Path(input_api.PresubmitLocalPath())
  crossbench_test_path = testing_path / "tests" / "crossbench"
  testing_env["PYTHONPATH"] = input_api.os_path.pathsep.join(
      map(str, [testing_path, crossbench_test_path]))
  # ---------------------------------------------------------------------------
  # Validate the vpython spec
  if platform.system() in ("Linux", "Darwin"):
    tests += input_api.canned_checks.CheckVPythonSpec(input_api, output_api)
  # ---------------------------------------------------------------------------
  if on_commit:
    files_to_check = [r"^[^\.]+\.py$"]
  else:
    # By default, the pylint canned check lints all Python files together to
    # check for potential problems between dependencies. This is slow to run
    # across all of crossbench (>2 min), so only lint affected files.
    files_to_check = ModifiedFiles(input_api)
  tests += input_api.canned_checks.GetPylint(
      input_api,
      output_api,
      files_to_check=files_to_check,
      pylintrc=".pylintrc",
      version="2.17")
  # ---------------------------------------------------------------------------
  # License header checks
  results += input_api.canned_checks.CheckLicense(input_api, output_api)
  # ---------------------------------------------------------------------------
  # Only run test_cli to speed up the presubmit checks
  if on_commit:
    dirs_to_check = crossbench_test_path.glob("**")
    files_to_check = [r".*test_.*\.py$"]
  else:
    # Only check a small subset on upload
    dirs_to_check = [crossbench_test_path / "cli"]
    files_to_check = [r".*test_cli_fast_.*\.py$"]
  for dir_to_check in dirs_to_check:
    # Skip potentially empty dirs
    if dir_to_check.name == "__pycache__":
      continue
    # End-to-end tests require custom setup and are not suited for presubmits.
    if "end2end" in dir_to_check.parts:
      continue
    tests += input_api.canned_checks.GetUnitTestsInDirectory(
        input_api,
        output_api,
        directory=dir_to_check,
        env=testing_env,
        files_to_check=files_to_check,
        skip_shebang_check=True,
        run_on_python2=False)
  # ---------------------------------------------------------------------------
  # Run all test
  results += input_api.RunTests(tests)
  return results


def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api, on_commit=False)


def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api, on_commit=True)
