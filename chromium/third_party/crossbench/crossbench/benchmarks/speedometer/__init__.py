# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from .speedometer_2_0 import Speedometer20Benchmark
from .speedometer_2_1 import Speedometer21Benchmark
from .speedometer_3_0 import Speedometer30Benchmark

benchmark_classes = [
    Speedometer20Benchmark, Speedometer21Benchmark, Speedometer30Benchmark
]

_versions = set()
for benchmark_cls in benchmark_classes:
  assert benchmark_cls.version() not in _versions, (
      f"Got duplicated benchmark version for {benchmark_cls}")
  _versions.add(benchmark_cls.version())
