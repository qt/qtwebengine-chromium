# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import datetime as dt
from typing import TYPE_CHECKING, ClassVar, Type

from typing_extensions import override

from crossbench.benchmarks.motionmark.motionmark_1 import \
    MotionMark1Benchmark, MotionMark1Probe, MotionMark1ProbeContext, \
    MotionMark1Story

if TYPE_CHECKING:
  from crossbench.benchmarks.base import VersionParts


class MotionMark131Probe(MotionMark1Probe):
  __doc__ = MotionMark1Probe.__doc__
  NAME: ClassVar = "motionmark_1.3.1"

  @override
  def get_context_cls(self) -> Type[MotionMark131ProbeContext]:
    return MotionMark131ProbeContext


class MotionMark131ProbeContext(MotionMark1ProbeContext):
  pass


class MotionMark131Story(MotionMark1Story):
  NAME: ClassVar = "motionmark_1.3.1"
  URL: ClassVar[
      str] = "https://chromium-workloads.web.app/motionmark/v1.3.1/MotionMark"
  URL_OFFICIAL: ClassVar[str] = "https://browserbench.org/MotionMark1.3.1"
  READY_TIMEOUT: ClassVar[dt.timedelta] = dt.timedelta(seconds=12)
  DEVELOPER_READY_JS: ClassVar[str] = (
      "return !(document.querySelector('#frame-rate-detection span'));")
  READY_JS: ClassVar[str] = (
      "return !!("
      "   document.querySelector('#frame-rate-label')?.textContent?.trim());")


class MotionMark131Benchmark(MotionMark1Benchmark):
  """
  Benchmark runner for MotionMark 1.3.1.

  See https://browserbench.org/MotionMark1.3.1/ for more details.
  """

  NAME: ClassVar = "motionmark_1.3.1"
  DEFAULT_STORY_CLS: ClassVar = MotionMark131Story
  PROBES: ClassVar = (MotionMark131Probe,)

  @classmethod
  @override
  def version(cls) -> VersionParts:
    return (1, 3, 1)
