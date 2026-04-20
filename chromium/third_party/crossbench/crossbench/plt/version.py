# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import abc
import re
from typing import Iterable, Self

from typing_extensions import override

from crossbench.helper.version import Version

VERSION_PART_RE = re.compile(r"[^0-9]*(?P<parts>[\d+.]+)")


class PlatformVersion(Version, metaclass=abc.ABCMeta):

  @classmethod
  def parse(cls, version_str: str) -> Self:
    match = VERSION_PART_RE.match(version_str)
    if not match:
      raise cls.parse_error("invalid version str", version_str)
    parts = [int(p) for p in match["parts"].split(".")]
    return cls(parts, version_str)

  @classmethod
  @override
  def _validate_parts(cls, parts: Iterable[int], value: str) -> tuple[int, ...]:
    parts = super()._validate_parts(parts, value)
    if not parts:
      raise cls.parse_error("missing parts", value)
    return parts

  @property
  def major(self) -> int:
    return self.parts[0]
