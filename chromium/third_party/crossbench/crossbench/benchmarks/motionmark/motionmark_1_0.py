# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from typing import TYPE_CHECKING, ClassVar, Type

from typing_extensions import override

from crossbench.benchmarks.motionmark.motionmark_1 import \
    MotionMark1Benchmark, MotionMark1Probe, MotionMark1ProbeContext, \
    MotionMark1Story

if TYPE_CHECKING:
  from crossbench.benchmarks.base import VersionParts


class MotionMark10Probe(MotionMark1Probe):
  __doc__ = MotionMark1Probe.__doc__
  NAME: ClassVar = "motionmark_1.0"

  @override
  def get_context_cls(self) -> Type[MotionMark10ProbeContext]:
    return MotionMark10ProbeContext


class MotionMark10ProbeContext(MotionMark1ProbeContext):
  pass


class MotionMark10Story(MotionMark1Story):
  NAME: ClassVar[str] = "motionmark_1.0"
  URL: ClassVar[str] = (
      "https://chromium-workloads.web.app/motionmark/v1.0/MotionMark")
  URL_OFFICIAL: ClassVar[str] = "https://browserbench.org/MotionMark1.0"


class MotionMark10Benchmark(MotionMark1Benchmark):
  """
  Benchmark runner for MotionMark 1.0.

  See https://browserbench.org/MotionMark1.0/ for more details.
  """

  NAME: ClassVar[str] = "motionmark_1.0"
  DEFAULT_STORY_CLS: ClassVar = MotionMark10Story
  PROBES: ClassVar = (MotionMark10Probe,)

  @classmethod
  @override
  def version(cls) -> VersionParts:
    return (1, 0)
