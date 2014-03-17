# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for skia/

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

def GetPreferredTrySlaves(project, change):
  return [
    'linux_gpu',
    'win_gpu',
    'mac_gpu',
    'mac_gpu_retina',
    'linux_layout_rel',
    ]
