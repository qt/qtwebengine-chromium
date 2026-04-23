# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import functools
from typing import TYPE_CHECKING, ClassVar, Optional, Type

from typing_extensions import override

from crossbench.action_runner.action.action import ACTION_TIMEOUT
from crossbench.action_runner.action.action_type import ActionType
from crossbench.action_runner.action.base_tab_action import BaseTabAction
from crossbench.parse import ObjectParser
from crossbench.runner.run import Run

if TYPE_CHECKING:
  import datetime as dt
  import re

  from crossbench.action_runner.action.action import ActionT
  from crossbench.action_runner.base import ActionRunner
  from crossbench.config import ConfigParser
  from crossbench.runner.run import Run


class OpenDevToolsAction(BaseTabAction):
  TYPE: ClassVar[ActionType] = ActionType.OPEN_DEVTOOLS

  def __init__(self,
               panel_name: Optional[str] = None,
               tab_index: Optional[int] = None,
               relative_tab_index: Optional[int] = None,
               title: Optional[re.Pattern] = None,
               url: Optional[re.Pattern] = None,
               timeout: dt.timedelta = ACTION_TIMEOUT,
               index: int = 0) -> None:
    self._panel_name = panel_name
    super().__init__(tab_index, relative_tab_index, title, url, timeout, index)

  @property
  def panel_name(self) -> str:
    return self._panel_name or "elements"

  @classmethod
  @override
  @functools.lru_cache(maxsize=1)
  def config_parser(cls: Type[ActionT]) -> ConfigParser[ActionT]:
    parser = super().config_parser()
    parser.add_argument("panel_name", type=ObjectParser.non_empty_str)
    return parser

  @override
  def run_with(self, run: Run, action_runner: ActionRunner) -> None:
    action_runner.open_devtools(run, self)
