# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for agents.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

USE_PYTHON3 = True
PRESUBMIT_VERSION = '2.0.0'



def CheckPatchFormatted(input_api, output_api):
    return input_api.canned_checks.CheckPatchFormatted(input_api, output_api)


def CheckPylint(input_api, output_api):
    return input_api.canned_checks.RunPylint(
        input_api,
        output_api,
        version='3.2',
    )
