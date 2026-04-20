# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from typing import TYPE_CHECKING, ClassVar, Optional

from crossbench.action_runner.action.action import ACTION_TIMEOUT
from crossbench.action_runner.action.action_type import ActionType
from crossbench.action_runner.action.base_probe import BaseProbeAction

if TYPE_CHECKING:
  import datetime as dt


# Left here for backwards compatibility.
# New probe actions should not have individual class implementations.
# They should just be used as ProbeActions directly.
class DumpHtmlAction(BaseProbeAction):
  TYPE: ClassVar[ActionType] = ActionType.DUMP_HTML

  def __init__(self,
               suffix: Optional[str] = None,
               timeout: dt.timedelta = ACTION_TIMEOUT,
               index: int = 0) -> None:
    kwargs = {}
    if suffix:
      kwargs["suffix"] = suffix
    super().__init__(
        probe="dump_html", kwargs=kwargs, timeout=timeout, index=index)
