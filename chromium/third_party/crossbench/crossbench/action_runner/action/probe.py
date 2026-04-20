# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import functools
from typing import TYPE_CHECKING, ClassVar, Type

from typing_extensions import override

from crossbench.action_runner.action.action_type import ActionType
from crossbench.action_runner.action.base_probe import BaseProbeAction
from crossbench.cli.config.probe import PROBE_LOOKUP
from crossbench.parse import ObjectParser

if TYPE_CHECKING:
  from crossbench.action_runner.action.action import ActionT
  from crossbench.config import ConfigParser


class ProbeAction(BaseProbeAction):
  TYPE: ClassVar[ActionType] = ActionType.PROBE

  @classmethod
  @override
  @functools.cache
  def config_parser(cls: Type[ActionT]) -> ConfigParser[ActionT]:
    parser = super().config_parser()
    parser.add_argument(
        "probe",
        type=ObjectParser.non_empty_str,
        required=True,
        choices=PROBE_LOOKUP.keys())
    parser.add_argument("kwargs", type=ObjectParser.dict, default={})
    return parser
