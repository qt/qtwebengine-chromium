# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import datetime as dt
import logging
from typing import Final

DATETIME_FORMAT: Final[str] = "%Y-%m-%d %H:%M:%S"


def format_time(time_str: str) -> str:
  try:
    time = dt.datetime.strptime(time_str, DATETIME_FORMAT)
    now = dt.datetime.now()
    if time.date() == now.date():
      return f"Today {time.strftime('%H:%M')}"
    if time.year == now.year:
      return time.strftime("%b-%d %H:%M")
    return time.strftime("%Y-%b-%d %H:%M")
  except ValueError:
    logging.warning("Invalid time format: %s", time_str)
    return time_str
