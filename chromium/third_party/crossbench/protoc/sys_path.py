# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from __future__ import annotations

import sys
from contextlib import contextmanager
from pathlib import Path
from typing import Iterator

MODULE_DIR = Path(__file__).parent.resolve()
GEN_DIR = MODULE_DIR / "gen"


@contextmanager
def protoc_in_sys_path() -> Iterator[None]:
  prev_path = sys.path
  sys.path = [str(GEN_DIR)] + prev_path
  try:
    yield
  finally:
    sys.path = prev_path
