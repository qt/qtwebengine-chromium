# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from typing import Tuple

from .motionmark_1 import (MotionMark1Benchmark, MotionMark1Probe,
                           MotionMark1Story)


class MotionMark13Probe(MotionMark1Probe):
  __doc__ = MotionMark1Probe.__doc__
  NAME = "motionmark_1.3"


class MotionMark13Story(MotionMark1Story):
  NAME = "motionmark_1.3"
  PROBES = (MotionMark13Probe,)
  URL = "https://browserbench.org/MotionMark1.3/developer.html"


class MotionMark13Benchmark(MotionMark1Benchmark):
  """
  Benchmark runner for MotionMark 1.3.

  See https://browserbench.org/MotionMark1.3/ for more details.
  """

  NAME = "motionmark_1.3"
  DEFAULT_STORY_CLS = MotionMark13Story

  @classmethod
  def version(cls) -> Tuple[int, ...]:
    return (1, 3)
