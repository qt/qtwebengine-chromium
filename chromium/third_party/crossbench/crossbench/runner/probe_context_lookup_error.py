# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from typing import TYPE_CHECKING, Final, Type

if TYPE_CHECKING:
  from crossbench.probes.probe import Probe


class ProbeContextLookupError(LookupError):

  def __init__(self, probe_cls: Type[Probe]) -> None:
    self._probe_cls: Final[Type[Probe]] = probe_cls
    super().__init__(
        f"No active probe context for {repr(probe_cls.NAME)} probe")
