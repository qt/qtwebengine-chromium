# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from typing import Final

from crossbench import path as pth

QUERIES_DIR: Final = pth.LocalPath(__file__).parent / "queries"
MODULES_DIR: Final = pth.LocalPath(__file__).parent / "modules/ext"
PROBE_NAME: Final[str] = "trace_processor"
