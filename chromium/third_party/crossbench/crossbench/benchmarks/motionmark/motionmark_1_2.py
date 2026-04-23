# Copyright 2022 The Chromium Authors
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


class MotionMark12Probe(MotionMark1Probe):
  __doc__ = MotionMark1Probe.__doc__
  NAME: ClassVar = "motionmark_1.2"

  @override
  def get_context_cls(self) -> Type[MotionMark12ProbeContext]:
    return MotionMark12ProbeContext


class MotionMark12ProbeContext(MotionMark1ProbeContext):
  pass


class MotionMark12Story(MotionMark1Story):
  NAME: ClassVar = "motionmark_1.2"
  URL: ClassVar[
      str] = "https://chromium-workloads.web.app/motionmark/v1.2/MotionMark"
  URL_OFFICIAL: ClassVar[str] = "https://browserbench.org/MotionMark1.2"


class MotionMark12Benchmark(MotionMark1Benchmark):
  """
  Benchmark runner for MotionMark 1.2.

  See https://browserbench.org/MotionMark1.2/ for more details.
  """

  NAME: ClassVar = "motionmark_1.2"
  DEFAULT_STORY_CLS: ClassVar = MotionMark12Story
  PROBES: ClassVar = (MotionMark12Probe,)

  @classmethod
  @override
  def version(cls) -> VersionParts:
    return (1, 2)
