# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import functools
from typing import TYPE_CHECKING, Any, Type

from immutabledict import immutabledict
from typing_extensions import override

from crossbench.action_runner.action.action import ACTION_TIMEOUT, Action, \
    ActionT
from crossbench.action_runner.action.action_type import ActionType
from crossbench.cli.config.probe import PROBE_LOOKUP

if TYPE_CHECKING:
  import datetime as dt

  from crossbench.action_runner.base import ActionRunner
  from crossbench.config import ConfigParser
  from crossbench.probes.probe import Probe
  from crossbench.runner.run import Run
  from crossbench.types import JsonDict


class BaseProbeAction(Action):

  @classmethod
  @override
  @functools.cache
  def config_parser(cls: Type[ActionT]) -> ConfigParser[ActionT]:
    parser = super().config_parser()
    return parser

  def __init__(self,
               probe: str,
               kwargs: dict[str, Any],
               timeout: dt.timedelta = ACTION_TIMEOUT,
               index: int = 0) -> None:
    self._probe_cls = PROBE_LOOKUP[probe]
    self._kwargs: immutabledict[str, Any] = immutabledict(kwargs)
    super().__init__(timeout, index)

  @property
  def probe_cls(self) -> Type[Probe]:
    return self._probe_cls

  @property
  def kwargs(self) -> immutabledict[str, Any]:
    return self._kwargs

  @override
  def run_with(self, run: Run, action_runner: ActionRunner) -> None:
    action_runner.invoke_probe(run, self)

  def kwargs_to_json(self) -> JsonDict:
    return dict(self.kwargs)

  @override
  def to_json(self) -> JsonDict:
    details = super().to_json()
    # Some legacy action types derive from this class and have a different
    # action type. For serialization purposes, force the type to be Probe
    # action.
    details["type"] = str(ActionType.PROBE)
    details["probe"] = self.probe_cls.NAME
    details["kwargs"] = self.kwargs_to_json()
    return details
