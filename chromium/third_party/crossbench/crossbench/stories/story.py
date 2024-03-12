# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import abc
import datetime as dt
import logging
from typing import TYPE_CHECKING, Sequence, Tuple, Type

from crossbench import plt

if TYPE_CHECKING:
  from crossbench.probes.probe import Probe
  from crossbench.runner.run import Run
  from crossbench.types import JsonDict


class Story(abc.ABC):
  PROBES: Tuple[Type[Probe], ...] = ()

  @classmethod
  @abc.abstractmethod
  def all_story_names(cls) -> Sequence[str]:
    pass

  def __init__(self,
               name: str,
               duration: dt.timedelta = dt.timedelta(seconds=15)):
    assert name, "Invalid page name"
    self._name = plt.safe_filename(name)
    self._duration = duration
    assert self._duration.total_seconds() > 0, (
        f"Duration must be non-empty, but got: {duration}")

  @property
  def name(self) -> str:
    return self._name

  @property
  def duration(self) -> dt.timedelta:
    return self._duration

  def details_json(self) -> JsonDict:
    return {"name": self.name, "duration": self.duration.total_seconds()}

  def log_run_details(self, run: Run) -> None:
    logging.info("STORY: %s", self)
    timing = run.timing
    logging.info("STORY DURATION: expected=%s timeout=%s",
                 timing.timedelta(self.duration),
                 timing.timeout_timedelta(self.duration))

  def setup(self, run: Run) -> None:
    """Setup work for a story that is not part of the main workload should
    be put in this method. Probes can skip measuring this section.
    i.e selecting substories to run.
    """

  @abc.abstractmethod
  def run(self, run: Run) -> None:
    """The main workload of a story that is measured by all Probes.
    """

  def tear_down(self, run: Run) -> None:
    """Cleanup work for a story that is not part of the main workload should
    be put in this method. Probes can skip measuring this section.
    """

  def __str__(self) -> str:
    return f"Story(name={self.name})"
