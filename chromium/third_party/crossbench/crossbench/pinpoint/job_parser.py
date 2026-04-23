# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import argparse
import re


def parse_job_id(value: str) -> str:
  value = value.strip().lower()
  parts = [p for p in value.split("/") if p]
  if not parts or not re.fullmatch(r"[0-9a-f]+", parts[-1]):
    raise argparse.ArgumentTypeError(f"Invalid job ID: {value}")
  return parts[-1]
