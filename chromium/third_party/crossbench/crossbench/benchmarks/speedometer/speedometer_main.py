# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from typing import TYPE_CHECKING, Type

from typing_extensions import override

from crossbench.benchmarks.speedometer.speedometer_3 import (
    Speedometer3Benchmark, Speedometer3Probe, Speedometer3ProbeContext,
    Speedometer3Story)

if TYPE_CHECKING:
  from crossbench.benchmarks.base import VersionParts
  from crossbench.benchmarks.speedometer.speedometer import ProbeClsTupleT


class SpeedometerMainProbe(Speedometer3Probe):
  """
  Speedometer3-specific probe (compatible with the main version).
  Extracts all speedometer times and scores.
  """
  NAME: str = "speedometer_main"

  @override
  def get_context_cls(self) -> Type[SpeedometerMainProbeContext]:
    return SpeedometerMainProbeContext


class SpeedometerMainProbeContext(Speedometer3ProbeContext):
  pass


class SpeedometerMainStory(Speedometer3Story):
  __doc__ = Speedometer3Story.__doc__
  NAME: str = "speedometer_main"
  URL: str = "https://chromium-workloads.web.app/speedometer/main/"
  URL_OFFICIAL: str = "https://chromium-workloads.web.app/speedometer/main/"


class SpeedometerMainBenchmark(Speedometer3Benchmark):
  """
  Benchmark runner for the Speedometer main version.
  """
  NAME: str = "speedometer_main"
  DEFAULT_STORY_CLS = SpeedometerMainStory  # type: ignore
  PROBES: ProbeClsTupleT = (SpeedometerMainProbe,)

  @classmethod
  @override
  def version(cls) -> VersionParts:
    # Using fake next version as a hack.
    return ("main",)
