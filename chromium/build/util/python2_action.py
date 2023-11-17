# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script for ensuring that a python action runs under Python2, not Python3."""

import subprocess
import sys

exe = sys.executable
sys.exit(subprocess.call([exe] + sys.argv[1:]))
