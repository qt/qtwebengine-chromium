# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from typing import Tuple

from .motionmark_1 import (MotionMark1Benchmark, MotionMark1Probe,
                           MotionMark1Story)


class MotionMark12Probe(MotionMark1Probe):
  __doc__ = MotionMark1Probe.__doc__
  NAME = "motionmark_1.2"


class MotionMark12Story(MotionMark1Story):
  NAME = "motionmark_1.2"
  PROBES = (MotionMark12Probe,)
  URL = "https://browserbench.org/MotionMark1.2/developer.html"


class MotionMark12Benchmark(MotionMark1Benchmark):
  """
  Benchmark runner for MotionMark 1.2.

  See https://browserbench.org/MotionMark1.2/ for more details.
  """

  NAME = "motionmark_1.2"
  DEFAULT_STORY_CLS = MotionMark12Story

  @classmethod
  def version(cls) -> Tuple[int, ...]:
    return (1, 2)
