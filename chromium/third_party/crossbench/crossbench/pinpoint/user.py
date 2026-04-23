# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import argparse
from enum import Enum


class UserEnum(Enum):
  ME = "me"
  ALL = "all"


def list_user(value: str) -> UserEnum | str:
  value_lower = value.lower()
  if value_lower == "me":
    return UserEnum.ME
  if value_lower == "all":
    return UserEnum.ALL
  if "@" in value:
    return value
  raise argparse.ArgumentTypeError(f"'{value}' is not a valid user email. "
                                   "Expected 'me', 'all', or an email address.")
