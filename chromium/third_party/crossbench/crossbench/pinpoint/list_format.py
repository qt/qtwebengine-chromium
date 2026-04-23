# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from enum import StrEnum


class ListFormatEnum(StrEnum):
  TABLE = "table"
  JSON = "json"
  YAML = "yaml"
  CSV = "csv"
  TSV = "tsv"
