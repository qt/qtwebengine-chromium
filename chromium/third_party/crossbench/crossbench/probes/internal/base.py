# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from typing import TYPE_CHECKING

from crossbench.probes.json import JsonResultProbe, JsonResultProbeContext
from crossbench.probes.probe import Probe

if TYPE_CHECKING:
  from crossbench.probes.results import ProbeResult
  from crossbench.runner.run import Run


class InternalProbe(Probe):
  IS_GENERAL_PURPOSE = False

  @property
  def is_internal(self) -> bool:
    return True


class InternalJsonResultProbe(JsonResultProbe, InternalProbe):
  IS_GENERAL_PURPOSE = False
  FLATTEN = False

  def get_context(self, run: Run) -> InternalJsonResultProbeContext:
    return InternalJsonResultProbeContext(self, run)


class InternalJsonResultProbeContext(
    JsonResultProbeContext[InternalJsonResultProbe]):

  def stop(self) -> None:
    # Only extract data in the late teardown phase.
    pass

  def teardown(self) -> ProbeResult:
    self._json_data = self.extract_json(self.run)  # pylint: disable=no-member
    return super().teardown()
