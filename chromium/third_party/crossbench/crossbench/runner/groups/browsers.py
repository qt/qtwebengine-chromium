# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from typing import TYPE_CHECKING, Iterable

from .base import RunGroup
from .repetitions import RepetitionsRunGroup

if TYPE_CHECKING:
  from crossbench import exception
  from crossbench.browsers.browser import Browser
  from crossbench.probes.probe import Probe
  from crossbench.probes.results import ProbeResult
  from crossbench.runner.groups.stories import StoriesRunGroup
  from crossbench.runner.run import Run


class BrowsersRunGroup(RunGroup):
  _story_groups: Iterable[StoriesRunGroup]

  def __init__(self, story_groups, throw: bool) -> None:
    super().__init__(throw)
    self._story_groups = story_groups
    self._set_path(story_groups[0].path.parents[1])

  @property
  def story_groups(self) -> Iterable[StoriesRunGroup]:
    return self._story_groups

  @property
  def browsers(self) -> Iterable[Browser]:
    for story_group in self._story_groups:
      yield story_group.browser

  @property
  def repetitions_groups(self) -> Iterable[RepetitionsRunGroup]:
    for story_group in self._story_groups:
      yield from story_group.repetitions_groups

  @property
  def runs(self) -> Iterable[Run]:
    for group in self._story_groups:
      yield from group.runs

  @property
  def info_stack(self) -> exception.TInfoStack:
    return ("Merging results from multiple browsers",)

  def _merge_probe_results(self, probe: Probe) -> ProbeResult:
    return probe.merge_browsers(self)  # pytype: disable=wrong-arg-types
