# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import datetime as dt
from typing import TYPE_CHECKING, Optional, Tuple, Type

from crossbench.action_runner.action.action import ACTION_TIMEOUT, ActionT
from crossbench.action_runner.action.action_type import ActionType
from crossbench.action_runner.action.base_input_source import InputSourceAction
from crossbench.benchmarks.loading.input_source import InputSource
from crossbench.benchmarks.loading.point import Point
from crossbench.parse import DurationParser, NumberParser, ObjectParser

if TYPE_CHECKING:
  from crossbench.action_runner.base import ActionRunner
  from crossbench.config import ConfigParser
  from crossbench.runner.run import Run
  from crossbench.types import JsonDict


class ClickAction(InputSourceAction):
  TYPE: ActionType = ActionType.CLICK

  @classmethod
  def config_parser(cls: Type[ActionT]) -> ConfigParser[ActionT]:
    parser = super().config_parser()
    parser.add_argument("selector", type=ObjectParser.non_empty_str)
    parser.add_argument("required", type=ObjectParser.bool, default=False)
    parser.add_argument(
        "scroll_into_view", type=ObjectParser.bool, default=False)
    parser.add_argument("x", type=NumberParser.positive_zero_int)
    parser.add_argument("y", type=NumberParser.positive_zero_int)
    parser.add_argument(
        "duration",
        type=DurationParser.positive_or_zero_duration,
        default=dt.timedelta())
    return parser

  def __init__(self,
               source: InputSource,
               duration: dt.timedelta = dt.timedelta(),
               selector: Optional[str] = None,
               required: bool = False,
               scroll_into_view: bool = False,
               x: Optional[int] = None,
               y: Optional[int] = None,
               timeout: dt.timedelta = ACTION_TIMEOUT,
               index: int = 0):
    # TODO: convert to custom selector object.
    self._selector = selector
    self._required: bool = required
    self._scroll_into_view: bool = scroll_into_view
    self._coordinates: Optional[Point] = None
    if x is not None and y is not None:
      self._coordinates = Point(x, y)
    super().__init__(source, duration, timeout, index)

  @property
  def selector(self) -> Optional[str]:
    return self._selector

  @property
  def required(self) -> bool:
    return self._required

  @property
  def scroll_into_view(self) -> bool:
    return self._scroll_into_view

  @property
  def coordinates(self) -> Optional[Point]:
    return self._coordinates

  def run_with(self, run: Run, action_runner: ActionRunner) -> None:
    action_runner.click(run, self)

  def validate(self) -> None:
    super().validate()

    if self._selector and self._coordinates:
      raise ValueError("Only one is allowed: either selector or coordinates")

    if not self._selector and not self._coordinates:
      raise ValueError("Either selector or coordinates are required")

    if self._input_source is InputSource.JS and self._coordinates:
      raise ValueError("X,Y Coordinates cannot be used with JS click source.")

    if self._required and self._coordinates:
      raise ValueError("'required' is not compatible with coordinates")

    if self._scroll_into_view and self._coordinates:
      raise ValueError("'scroll_into_view' is not compatible with coordinates")

  def validate_duration(self) -> None:
    # A click action is allowed to have a zero duration.
    return

  def supported_input_sources(self) -> Tuple[InputSource, ...]:
    return (InputSource.JS, InputSource.TOUCH, InputSource.MOUSE)

  def to_json(self) -> JsonDict:
    details = super().to_json()

    if self._selector:
      details["selector"] = self._selector
      details["required"] = self._required
      details["scroll_into_view"] = self._scroll_into_view
    else:
      assert self._coordinates
      details["x"] = self._coordinates.x
      details["y"] = self._coordinates.y
    return details
