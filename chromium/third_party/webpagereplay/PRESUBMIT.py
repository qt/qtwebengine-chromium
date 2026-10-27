# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for changes affecting webpagereplay.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

from __future__ import annotations

import pathlib
import sys

PRESUBMIT_VERSION = '2.0.0'
USE_PYTHON3 = True


def CheckGoTests(input_api, output_api):
    sys.path.append(
        str(pathlib.Path(input_api.PresubmitLocalPath()) / 'scripts'))
    import go_utils
    sys.path.pop()

    go_path = str(go_utils.get_go_compiler_path())
    src_path = str(pathlib.Path(input_api.PresubmitLocalPath()) / 'src')
    return input_api.RunTests([
        input_api.Command(name='webpagereplay package tests',
                          cmd=[go_path, 'test', './webpagereplay'],
                          kwargs={'cwd': src_path},
                          message=output_api.PresubmitError),
        input_api.Command(name='wpr.go tests',
                          cmd=[go_path, 'test', 'wpr.go', 'wpr_test.go'],
                          kwargs={'cwd': src_path},
                          message=output_api.PresubmitError),
        input_api.Command(
            name='httparchive tests',
            cmd=[go_path, 'test', 'httparchive.go', 'httparchive_test.go'],
            kwargs={'cwd': src_path},
            message=output_api.PresubmitError)
    ])


def CheckPanProjectChecks(input_api, output_api):
    # The code-owners plugin is not enabled on the webpagereplay gerrit host, so
    # owners_check is set to false to avoid a failure. Note that owners-approval
    # is still enforced in other manners.
    return input_api.canned_checks.PanProjectChecks(input_api,
                                                    output_api,
                                                    owners_check=False)


def CheckPythonAndJavascriptFormat(input_api, output_api):
    return input_api.canned_checks.CheckPatchFormatted(
        input_api,
        output_api,
        check_clang_format=True,
        check_js=True,
        check_python=True,
        result_factory=output_api.PresubmitError)


def CheckGoFormat(input_api, output_api):
    return input_api.RunTests([
        input_api.Command(name='Checking go format',
                          cmd=['scripts/check_go_format.py'],
                          kwargs={'cwd': input_api.PresubmitLocalPath()},
                          message=output_api.PresubmitError)
    ])


def CheckRuff(input_api, output_api):
    return input_api.RunTests([
        input_api.Command(name='Checking ruff format',
                          cmd=[
                              'vpython3', '-m', 'ruff', 'check', '--select',
                              'E,F,W,B,I,UP', '--config', 'exclude=[]'
                          ],
                          kwargs={'cwd': input_api.PresubmitLocalPath()},
                          message=output_api.PresubmitError)
    ])
