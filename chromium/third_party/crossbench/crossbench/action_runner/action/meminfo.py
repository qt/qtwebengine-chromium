# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import functools
from typing import TYPE_CHECKING, ClassVar, Optional, Type

from typing_extensions import override

from crossbench.action_runner.action.action import ACTION_TIMEOUT, ActionT
from crossbench.action_runner.action.action_type import ActionType
from crossbench.action_runner.action.probe import BaseProbeAction
from crossbench.parse import ObjectParser

if TYPE_CHECKING:
  import datetime as dt

  from crossbench.config import ConfigParser


# Left here for backwards compatibility.
# New probe actions should not have individual class implementations.
# They should just be used as ProbeActions directly.
class MeminfoAction(BaseProbeAction):
  TYPE: ClassVar[ActionType] = ActionType.MEMINFO

  @classmethod
  @override
  @functools.lru_cache(maxsize=1)
  def config_parser(cls: Type[ActionT]) -> ConfigParser[ActionT]:
    parser = super().config_parser()
    parser.add_argument("browser", type=ObjectParser.bool, default=True)
    parser.add_argument(
        "packages", type=ObjectParser.non_empty_str, default=(), is_list=True)
    parser.add_argument("system", type=ObjectParser.bool, default=False)
    parser.add_argument("title", type=ObjectParser.non_empty_str, default=None)
    return parser

  def __init__(self,
               browser: bool = True,
               packages: tuple[str, ...] = (),
               title: Optional[str] = None,
               system: bool = False,
               timeout: dt.timedelta = ACTION_TIMEOUT,
               index: int = 0) -> None:
    kwargs = {
        "browser": browser,
        "system": system,
        "packages": packages,
        "title": title
    }
    super().__init__(
        probe="meminfo", kwargs=kwargs, timeout=timeout, index=index)
