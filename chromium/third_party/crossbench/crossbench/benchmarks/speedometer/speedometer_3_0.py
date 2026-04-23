# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from typing import TYPE_CHECKING, ClassVar, Type

from typing_extensions import override

from crossbench.benchmarks.speedometer.speedometer_3 import \
    Speedometer3Benchmark, Speedometer3Probe, Speedometer3ProbeContext, \
    Speedometer3Story

if TYPE_CHECKING:
  from crossbench.benchmarks.base import VersionParts
  from crossbench.benchmarks.speedometer.speedometer import ProbeClsTupleT


class Speedometer30Probe(Speedometer3Probe):
  """
  Speedometer3-specific probe (compatible with v3.0).
  Extracts all speedometer times and scores.
  """
  NAME: ClassVar[str] = "speedometer_3.0"

  @override
  def get_context_cls(self) -> Type[Speedometer30ProbeContext]:
    return Speedometer30ProbeContext


class Speedometer30ProbeContext(Speedometer3ProbeContext):
  pass


class Speedometer30Story(Speedometer3Story):
  __doc__ = Speedometer3Story.__doc__
  NAME: ClassVar[str] = "speedometer_3.0"
  URL: ClassVar[str] = "https://chromium-workloads.web.app/speedometer/v3.0/"
  URL_OFFICIAL: ClassVar[str] = "https://browserbench.org/Speedometer3.0/"
  URL_CHROME_FORK: ClassVar[
      str] = "https://chromium-workloads.web.app/speedometer/v3.0-custom/"


class Speedometer30Benchmark(Speedometer3Benchmark):
  """
  Benchmark runner for Speedometer 3.0
  """
  NAME: ClassVar[str] = "speedometer_3.0"
  DEFAULT_STORY_CLS: ClassVar = Speedometer30Story  # type: ignore
  PROBES: ClassVar[ProbeClsTupleT] = (Speedometer30Probe,)

  @classmethod
  @override
  def version(cls) -> VersionParts:
    return (3, 0)
