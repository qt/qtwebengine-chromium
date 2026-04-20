# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from typing import IO, Any, Iterable

import hjson


def _check_for_duplicate_keys(
    key_values: Iterable[tuple[str, Any]]) -> dict[str, Any]:
  result = {}
  for key, value in key_values:
    if key in result:
      raise ValueError(f"Duplicate key in hjson: {key}")
    result[key] = value
  return result


def load_unique_keys(file: IO[str]) -> Any:
  return hjson.load(
      file, object_pairs_hook=_check_for_duplicate_keys)  # type: ignore


def loads_unique_keys(string_value: str) -> Any:
  return hjson.loads(string_value, object_pairs_hook=_check_for_duplicate_keys)
