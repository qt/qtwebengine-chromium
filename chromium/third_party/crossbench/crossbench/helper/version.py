# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import functools
from typing import Any, Final, Iterable


class VersionParseError(ValueError):

  def __init__(self, name: str, msg: str, version_str: str) -> None:
    self._version_str = version_str
    super().__init__(f"Invalid {name} {repr(version_str)}: {msg}")

  @property
  def version_str(self) -> str:
    return self._version_str


@functools.total_ordering
class Version:
  _MAX_PART_VALUE: Final[int] = 0xFFFF

  @classmethod
  def parse_error(cls, msg: str, version: str) -> VersionParseError:
    return VersionParseError(cls.__name__, msg, version)

  def __init__(self, parts: Iterable[int], version_str: str = "") -> None:
    self._parts: Final[tuple[int, ...]] = self._validate_parts(
        parts, version_str or repr(parts))
    self._version_str: Final[str] = version_str

  @classmethod
  def _validate_parts(cls, parts: Iterable[int], value: str) -> tuple[int, ...]:
    if parts is None:
      raise cls.parse_error("Invalid version format", value)
    parts_tuple = tuple(parts)
    for part in parts_tuple:
      if part < 0:
        raise cls.parse_error("Version parts must be positive", value)
    return parts_tuple

  @property
  def parts(self) -> tuple[int, ...]:
    return self._parts

  @property
  def version_str(self) -> str:
    return self._version_str

  @property
  def parts_str(self) -> str:
    return ".".join(map(str, self._parts))

  def comparable_parts(self, padded_len: int) -> tuple[int, ...]:
    if self.is_complete:
      return self._parts
    padding = (self._MAX_PART_VALUE,) * (padded_len - len(self._parts))
    return self._parts + padding

  @property
  def is_complete(self) -> bool:
    return True

  def is_compatible_type(self, other: object) -> bool:
    if isinstance(other, type(self)):
      return True
    other_type = type(other)
    return issubclass(other_type, Version) and isinstance(self, other_type)

  def __hash__(self) -> int:
    return hash(self.key)

  def __eq__(self, other: object) -> bool:
    if not self.is_compatible_type(other):
      return False
    assert isinstance(other, Version)
    return self.key == other.key

  def __le__(self, other: Any) -> bool:
    if not self.is_compatible_type(other):
      raise TypeError("Cannot compare unrelated versions : "
                      f"{type(self).__name__} vs. "
                      f"{type(other).__name__}.")
    return self.key <= other.key

  @property
  def key(self) -> tuple[tuple[int, ...], Any]:
    return (self._parts, None)
