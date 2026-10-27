#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Script to run WebPageReplay wpr.go. This has a few advantages compared to a
direct go run:

1. Uses the bundled toolchain from DEPS, so it spares go installation. At the
same time, it spares knowing the details about the toolchain path, which is
architecture-dependent.

2. Can be run from outside the repo directory, while still resolving paths
   correctly.

   $ cd $CHROMIUM_SRC
   $ cat $CHROMIUM_SRC/input.txt
   Hi
   $ go run third_party/webpagereplay/src/wpr.go --some-input=input.txt
   Error: go.mod file not found
   $ go run -C third_party/webpagereplay src/wpr.go --some-input=input.txt
   Error: input.txt not found
   $ go run -C third_party/webpagereplay src/wpr.go --some-input=../../input.txt
   Success: input.txt found
   $ third_party/webpagereplay/script/run_wpr.py --some-input=input.txt
   Success: input.txt found

Note: why not make this run.py with --binary that can be "wpr" or "httparchive"?
To avoid handling run.py flags on top of the underlying binary flags.
"""

from __future__ import annotations

import pathlib
import subprocess
import sys
import tempfile

import build


def main():
    with tempfile.TemporaryDirectory() as tmpdir:
        build.build(out_dir=tmpdir)
        return_code = subprocess.call([str(pathlib.Path(tmpdir) / "wpr")] +
                                      sys.argv[1:])
        sys.exit(return_code)


if __name__ == "__main__":
    main()
