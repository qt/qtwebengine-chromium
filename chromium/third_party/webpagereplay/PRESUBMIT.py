# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for changes affecting webpagereplay.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import pathlib
import tempfile

PRESUBMIT_VERSION = '2.0.0'
USE_PYTHON3 = True


def CheckBuildpWpr(input_api, output_api):
    # Note: CheckGoTests() doesn't build the main function, that's why this
    # separate check exists.
    cmd_name = 'Test wpr builds'
    with tempfile.TemporaryDirectory() as tmpdir:
        out_path = str(pathlib.PurePath(tmpdir) / "wpr")
        test_cmd = input_api.Command(
            name=cmd_name,
            cmd=['go', 'build', '-o', out_path, './src/wpr.go'],
            kwargs={'cwd': input_api.PresubmitLocalPath()},
            message=output_api.PresubmitError)
        return input_api.RunTests([test_cmd])


def CheckBuildHttpArchive(input_api, output_api):
    # Note: CheckGoTests() doesn't build the main function, that's why this
    # separate check exists.
    cmd_name = 'Test httparchive builds'
    with tempfile.TemporaryDirectory() as tmpdir:
        out_path = str(pathlib.Path(tmpdir) / "httparchive")
        test_cmd = input_api.Command(
            name=cmd_name,
            cmd=['go', 'build', '-o', out_path, './src/httparchive.go'],
            kwargs={'cwd': input_api.PresubmitLocalPath()},
            message=output_api.PresubmitError)
        return input_api.RunTests([test_cmd])


def CheckGoTests(input_api, output_api):
    cmd_name = 'WebPageReplay go tests'
    if input_api.verbose:
        print(f'Running {cmd_name}')
    test_cmd = input_api.Command(
        name=cmd_name,
        cmd=['go', 'test', './webpagereplay'],
        kwargs={
            'cwd': str(pathlib.Path(input_api.PresubmitLocalPath()) / 'src')
        },
        message=output_api.PresubmitError)
    return input_api.RunTests([test_cmd])


def CheckPrebuiltBinaryUpdated(input_api, output_api):
    files = input_api.UnixLocalPaths()
    if (not any(f.endswith('binary_dependencies.json') for f in files)
            and any(f.endswith('.go') for f in files)):
        return [
            output_api.PresubmitError(
                'You changed go files, but didn\'t run scripts/'
                'upload_new_binaries.py')
        ]

    return []


def CheckPanProjectChecks(input_api, output_api):
    # The code-owners plugin is not enabled on the webpagereplay gerrit host, so
    # owners_check is set to false to avoid a failure. Note that owners-approval
    # is still enforced in other manners.
    return input_api.canned_checks.PanProjectChecks(input_api,
                                                    output_api,
                                                    owners_check=False)


def CheckPythonFormat(input_api, output_api):
    return input_api.canned_checks.CheckPatchFormatted(
        input_api,
        output_api,
        check_clang_format=False,
        check_js=False,
        check_python=True,
        result_factory=output_api.PresubmitError)


def CheckGoFormat(input_api, output_api):
    cmd_name = 'Checking go format'
    test_cmd = input_api.Command(
        name=cmd_name,
        cmd=['scripts/check_go_format.py'],
        kwargs={'cwd': input_api.PresubmitLocalPath()},
        message=output_api.PresubmitError)
    return input_api.RunTests([test_cmd])
