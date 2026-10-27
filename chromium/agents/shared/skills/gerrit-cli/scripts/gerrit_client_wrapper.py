#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper wrapper script to run gerrit_client.py from PATH."""

import shutil
import subprocess
import sys


def main():
    client = shutil.which('gerrit_client.py')
    if not client:
        print('gerrit_client.py not found. '
              'Is depot_tools available and added to PATH?')
        sys.exit(1)

    rc = subprocess.call(['vpython3', client] + sys.argv[1:])
    sys.exit(rc)


if __name__ == '__main__':
    main()
