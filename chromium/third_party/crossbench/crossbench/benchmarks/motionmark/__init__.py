# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from .motionmark_1_2 import MotionMark12Benchmark
from .motionmark_1_3 import MotionMark13Benchmark

benchmark_classes = (MotionMark12Benchmark, MotionMark13Benchmark)

_versions = set()
for benchmark_cls in benchmark_classes:
  assert benchmark_cls.version() not in _versions, (
      f"Got duplicated benchmark version for {benchmark_cls}")
  _versions.add(benchmark_cls.version())
