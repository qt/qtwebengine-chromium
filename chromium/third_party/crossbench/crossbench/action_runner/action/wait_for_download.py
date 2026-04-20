# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import functools
import re
from typing import TYPE_CHECKING, ClassVar, Type

from typing_extensions import override

from crossbench.action_runner.action.action import ACTION_TIMEOUT, ActionT
from crossbench.action_runner.action.action_type import ActionType
from crossbench.action_runner.action.base_probe import BaseProbeAction
from crossbench.parse import ObjectParser

if TYPE_CHECKING:
  import datetime as dt

  from crossbench.config import ConfigParser
  from crossbench.types import JsonDict


# Left here for backwards compatibility.
# New probe actions should not have individual class implementations.
# They should just be used as ProbeActions directly.
class WaitForDownloadAction(BaseProbeAction):
  TYPE: ClassVar[ActionType] = ActionType.WAIT_FOR_DOWNLOAD

  @classmethod
  @override
  @functools.lru_cache(maxsize=1)
  def config_parser(cls: Type[ActionT]) -> ConfigParser[ActionT]:
    parser = super().config_parser()
    parser.add_argument(
        "pattern",
        type=ObjectParser.regexp,
        help="A regexp to search downloaded file names",
        required=True)
    return parser

  def __init__(self,
               pattern: re.Pattern,
               timeout: dt.timedelta = ACTION_TIMEOUT,
               index: int = 0) -> None:
    kwargs = {
        "pattern": pattern,
    }
    super().__init__(
        probe="downloads", kwargs=kwargs, timeout=timeout, index=index)

  @override
  def kwargs_to_json(self) -> JsonDict:
    pattern = self.kwargs["pattern"]

    if isinstance(pattern, re.Pattern):
      pattern = pattern.pattern

    return {"pattern": pattern}
