# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from typing import TYPE_CHECKING

from crossbench.probes.internal.base import InternalJsonResultProbe
from crossbench.probes.results import EmptyProbeResult

if TYPE_CHECKING:
  from crossbench.probes.results import ProbeResult
  from crossbench.runner.actions import Actions
  from crossbench.runner.groups.repetitions import RepetitionsRunGroup
  from crossbench.types import Json


class SystemDetailsProbe(InternalJsonResultProbe):
  """
  Runner-internal meta-probe: Collects the browser's system/platform details.
  """
  NAME = "cb.system.details"

  def to_json(self, actions: Actions) -> Json:
    return actions.run.browser_platform.system_details()

  def merge_repetitions(self, group: RepetitionsRunGroup) -> ProbeResult:
    return EmptyProbeResult()
