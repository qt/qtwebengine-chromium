# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import abc
import argparse
import datetime as dt
import enum
import json
import logging
from typing import TYPE_CHECKING, Any, Dict, Optional, Tuple, Type

from crossbench import cli_helper
from crossbench.benchmarks.loading.action_runner.base import ActionRunner
from crossbench.config import ConfigEnum

if TYPE_CHECKING:
  import crossbench.path as pth
  from crossbench.runner.run import Run
  from crossbench.types import JsonDict


@enum.unique
class ActionType(ConfigEnum):
  GET: "ActionType" = ("get", "Open a URL")
  WAIT: "ActionType" = ("wait", "Wait for a given time")
  SCROLL: "ActionType" = ("scroll", "Scroll on page")
  CLICK: "ActionType" = ("click", "Click on element")
  TAP: "ActionType" = ("tap", "Tap on element")
  SWIPE: "ActionType" = ("swipe", "Swipe on screen")
  WAIT_FOR_ELEMENT: "ActionType" = ("wait_for_element",
                                    "Wait until element appears on the page")
  INJECT_NEW_DOCUMENT_SCRIPT: "ActionType" = ("inject_new_document_script", (
      "Evaluates given script in every frame upon creation "
      "(before loading frame's scripts). "
      "Only supported in chromium-based browsers."))


@enum.unique
class ButtonClick(ConfigEnum):
  LEFT: "ButtonClick" = ("left", "Press left mouse button")
  RIGHT: "ButtonClick" = ("right", "Press right mouse button")
  MIDDLE: "ButtonClick" = ("middle", "Press middle mouse button")


ACTION_TIMEOUT = dt.timedelta(seconds=20)


class Action(abc.ABC):
  TYPE: ActionType = ActionType.GET

  @classmethod
  def kwargs_from_dict(cls, value: JsonDict) -> Dict[str, Any]:
    kwargs = {}
    if timeout := value.pop("timeout", None):
      kwargs["timeout"] = cli_helper.Duration.parse_non_zero(timeout)
    return kwargs

  @classmethod
  def pop_required_input(cls, data: JsonDict, key: str) -> Any:
    if key not in data:
      raise argparse.ArgumentTypeError(
          f"{cls.__name__}: Missing '{key}' property in {json.dumps(data)}")
    value = data.pop(key)
    if value is None:
      raise argparse.ArgumentTypeError(
          f"{cls.__name__}: {key} should not be None")
    return value

  def __init__(self, timeout: dt.timedelta = ACTION_TIMEOUT):
    self._timeout: dt.timedelta = timeout
    self.validate()

  @property
  def duration(self) -> dt.timedelta:
    return dt.timedelta(milliseconds=10)

  @property
  def timeout(self) -> dt.timedelta:
    return self._timeout

  @property
  def has_timeout(self) -> bool:
    return self._timeout != dt.timedelta.max

  @abc.abstractmethod
  def run_with(self, run: Run, action_runner: ActionRunner) -> None:
    pass

  def validate(self) -> None:
    if self._timeout.total_seconds() < 0:
      raise ValueError(
          f"{self}.timeout should be positive, but got {self.timeout}")

  def to_json(self) -> JsonDict:
    return {"type": str(self.TYPE), "timeout": self.timeout.total_seconds()}

  def __eq__(self, other: object) -> bool:
    if isinstance(other, Action):
      return self.to_json() == other.to_json()
    return False


@enum.unique
class ReadyState(ConfigEnum):
  """See https://developer.mozilla.org/en-US/docs/Web/API/Document/readyState"""
  # Non-blocking:
  ANY: "ReadyState" = ("any", "Ignore ready state")
  # Blocking (on dom event):
  LOADING: "ReadyState" = ("loading", "The document is still loading.")
  INTERACTIVE: "ReadyState" = ("interactive",
                               "The document has finished loading "
                               "but sub-resources might still be loading")
  COMPLETE: "ReadyState" = (
      "complete", "The document and all sub-resources have finished loading.")


@enum.unique
class WindowTarget(ConfigEnum):
  """See https://developer.mozilla.org/en-US/docs/Web/API/Window/open"""
  SELF: "WindowTarget" = ("_self", "The current browsing context. (Default)")
  BLANK: "WindowTarget" = (
      "_blank", "Usually a new tab, but users can configure browsers "
      "to open a new window instead.")
  PARENT: "WindowTarget" = ("_parent",
                            "The parent browsing context of the current one. "
                            "If no parent, behaves as _self.")
  TOP: "WindowTarget" = (
      "_top", "The topmost browsing context "
      "(the 'highest' context that's an ancestor of the current one). "
      "If no ancestors, behaves as _self.")


class GetAction(Action):
  TYPE: ActionType = ActionType.GET

  @classmethod
  def kwargs_from_dict(cls, value: JsonDict) -> Dict[str, Any]:
    kwargs = super().kwargs_from_dict(value)
    kwargs["url"] = cli_helper.parse_url_str(
        cls.pop_required_input(value, "url"))
    if duration := value.pop("duration", None):
      kwargs["duration"] = cli_helper.Duration.parse_zero(duration)
    if ready_state := value.pop("ready-state", None):
      kwargs["ready_state"] = ReadyState.parse(ready_state)
    if target := value.pop("target", None):
      kwargs["target"] = WindowTarget.parse(target)
    return kwargs

  def __init__(self,
               url: str,
               duration: dt.timedelta = dt.timedelta(),
               timeout: dt.timedelta = ACTION_TIMEOUT,
               ready_state: ReadyState = ReadyState.ANY,
               target: WindowTarget = WindowTarget.SELF):
    if not url:
      raise ValueError(f"{self}.url is missing")
    self._url: str = url

    self._duration = duration
    if ready_state != ReadyState.ANY:
      if duration != dt.timedelta():
        raise ValueError(
            f"Expected empty duration with ReadyState {ready_state} "
            f"but got: {self.duration}")
      self._duration = dt.timedelta()
    self._ready_state = ready_state
    self._target = target
    super().__init__(timeout)

  @property
  def url(self) -> str:
    return self._url

  @property
  def ready_state(self) -> ReadyState:
    return self._ready_state

  @property
  def duration(self) -> dt.timedelta:
    return self._duration

  @property
  def target(self) -> WindowTarget:
    return self._target

  def run_with(self, run: Run, action_runner: ActionRunner) -> None:
    action_runner.get(run, self)

  def to_json(self) -> JsonDict:
    details = super().to_json()
    details["url"] = self.url
    details["duration"] = self.duration.total_seconds()
    details["ready_state"] = str(self.ready_state)
    details["target"] = str(self.target)
    return details


class DurationAction(Action):
  TYPE: ActionType = ActionType.WAIT

  @classmethod
  def kwargs_from_dict(cls, value: JsonDict) -> Dict[str, Any]:
    kwargs = super().kwargs_from_dict(value)
    kwargs["duration"] = cli_helper.Duration.parse_non_zero(
        cls.pop_required_input(value, "duration"))
    return kwargs

  def __init__(self,
               duration: dt.timedelta,
               timeout: dt.timedelta = ACTION_TIMEOUT) -> None:
    self._duration: dt.timedelta = duration
    super().__init__(timeout)

  @property
  def duration(self) -> dt.timedelta:
    return self._duration

  def validate(self) -> None:
    super().validate()
    if self.duration.total_seconds() <= 0:
      raise ValueError(
          f"{self}.duration should be positive, but got {self.duration}")

  def to_json(self) -> JsonDict:
    details = super().to_json()
    details["duration"] = self.duration.total_seconds()
    return details


class WaitAction(DurationAction):
  TYPE: ActionType = ActionType.WAIT

  def run_with(self, run: Run, action_runner: ActionRunner) -> None:
    action_runner.wait(run, self)


class ScrollAction(DurationAction):
  TYPE: ActionType = ActionType.SCROLL

  @classmethod
  def kwargs_from_dict(cls, value: JsonDict) -> Dict[str, Any]:
    kwargs = super().kwargs_from_dict(value)
    if distance := value.pop("distance", None):
      kwargs["distance"] = cli_helper.parse_float(distance)
    return kwargs

  def __init__(self,
               distance: float = 500.0,
               duration: dt.timedelta = dt.timedelta(seconds=1),
               timeout: dt.timedelta = ACTION_TIMEOUT) -> None:
    self._distance = distance
    super().__init__(duration, timeout)

  @property
  def distance(self) -> float:
    return self._distance

  def run_with(self, run: Run, action_runner: ActionRunner) -> None:
    action_runner.scroll(run, self)

  def validate(self) -> None:
    super().validate()
    if not self.distance:
      raise ValueError(f"{self}.distance is not provided")

  def to_json(self) -> JsonDict:
    details = super().to_json()
    details["distance"] = str(self.distance)
    return details


class ClickAction(Action):
  TYPE: ActionType = ActionType.CLICK

  @classmethod
  def kwargs_from_dict(cls, value: JsonDict) -> Dict[str, Any]:
    kwargs = super().kwargs_from_dict(value)
    kwargs["selector"] = cls.pop_required_input(value, "selector")
    if required := value.pop("required", None):
      kwargs["required"] = cli_helper.parse_bool(required)
    if scroll_into_view := value.pop("scroll_into_view", None):
      kwargs["scroll_into_view"] = cli_helper.parse_bool(scroll_into_view)
    return kwargs

  def __init__(self,
               selector: str,
               required: bool = False,
               scroll_into_view: bool = False,
               timeout: dt.timedelta = ACTION_TIMEOUT):
    # TODO: convert to custom selector object.
    self._selector = selector
    self._scroll_into_view: bool = scroll_into_view
    self._required: bool = required
    super().__init__(timeout)

  @property
  def scroll_into_view(self) -> bool:
    return self._scroll_into_view

  @property
  def selector(self) -> str:
    return self._selector

  @property
  def required(self) -> bool:
    return self._required

  def run_with(self, run: Run, action_runner: ActionRunner) -> None:
    action_runner.click(run, self)

  def validate(self) -> None:
    super().validate()
    if not self.selector:
      raise ValueError(f"{self}.selector is missing.")

  def to_json(self) -> JsonDict:
    details = super().to_json()
    details["selector"] = self.selector
    details["required"] = self.required
    details["scroll_into_view"] = self.scroll_into_view
    return details


class TapAction(Action):
  TYPE: ActionType = ActionType.TAP

  @classmethod
  def kwargs_from_dict(cls, value: JsonDict) -> Dict[str, Any]:
    kwargs = super().kwargs_from_dict(value)
    kwargs["selector"] = value.pop("selector", None)
    kwargs["x"] = value.pop("x", None)
    kwargs["y"] = value.pop("y", None)
    return kwargs

  def __init__(self,
               selector: Optional[str] = None,
               x: Optional[int] = None,
               y: Optional[int] = None,
               timeout: dt.timedelta = ACTION_TIMEOUT):
    # TODO: convert to custom selector object.
    self._selector = selector
    self._x = x
    self._y = y
    super().__init__(timeout)

  @property
  def selector(self) -> Optional[str]:
    return self._selector

  @property
  def x(self) -> Optional[int]:
    return self._x

  @property
  def y(self) -> Optional[int]:
    return self._y

  def run_with(self, run: Run, action_runner: ActionRunner) -> None:
    action_runner.tap(run, self)

  def validate(self) -> None:
    super().validate()
    if self.selector:
      if self.x is not None or self.y is not None:
        raise ValueError("Only one is allowed: either selector or coordinates")
    else:
      if self.x is None or self.y is None:
        raise ValueError("Both selector and coordinates are missing")

  def to_json(self) -> JsonDict:
    details = super().to_json()
    if self.selector:
      details["selector"] = self.selector
    else:
      details["x"] = self.x
      details["y"] = self.y
    return details


class SwipeAction(DurationAction):
  TYPE: ActionType = ActionType.SWIPE

  @classmethod
  def kwargs_from_dict(cls, value: JsonDict) -> Dict[str, Any]:
    kwargs = super().kwargs_from_dict(value)
    kwargs["startx"] = cls.pop_required_input(value, "startx")
    kwargs["starty"] = cls.pop_required_input(value, "starty")
    kwargs["endx"] = cls.pop_required_input(value, "endx")
    kwargs["endy"] = cls.pop_required_input(value, "endy")
    return kwargs

  def __init__(self,
               startx: int,
               starty: int,
               endx: int,
               endy: int,
               duration: dt.timedelta = dt.timedelta(seconds=1),
               timeout: dt.timedelta = ACTION_TIMEOUT) -> None:
    self._startx: int = startx
    self._starty: int = starty
    self._endx: int = endx
    self._endy: int = endy
    super().__init__(duration, timeout)

  @property
  def startx(self) -> int:
    return self._startx

  @property
  def starty(self) -> int:
    return self._starty

  @property
  def endx(self) -> int:
    return self._endx

  @property
  def endy(self) -> int:
    return self._endy

  def run_with(self, run: Run, action_runner: ActionRunner) -> None:
    action_runner.swipe(run, self)

  def to_json(self) -> JsonDict:
    details = super().to_json()
    details["startx"] = self._startx
    details["starty"] = self._starty
    details["endx"] = self._endx
    details["endy"] = self._endy
    return details


class WaitForElementAction(Action):
  TYPE: ActionType = ActionType.WAIT_FOR_ELEMENT

  @classmethod
  def kwargs_from_dict(cls, value: JsonDict) -> Dict[str, Any]:
    kwargs = super().kwargs_from_dict(value)
    kwargs["selector"] = cls.pop_required_input(value, "selector")
    return kwargs

  def __init__(self, selector: str, timeout: dt.timedelta = ACTION_TIMEOUT):
    self._selector = selector
    super().__init__(timeout)

  @property
  def selector(self) -> str:
    return self._selector

  def run_with(self, run: Run, action_runner: ActionRunner) -> None:
    action_runner.wait_for_element(run, self)

  def validate(self) -> None:
    super().validate()
    if not self.selector:
      raise ValueError(f"{self}.selector is missing.")

  def to_json(self) -> JsonDict:
    details = super().to_json()
    details["selector"] = self.selector
    return details


class InjectNewDocumentScriptAction(Action):
  TYPE: ActionType = ActionType.INJECT_NEW_DOCUMENT_SCRIPT

  @classmethod
  def kwargs_from_dict(cls, value: JsonDict) -> Dict[str, Any]:
    kwargs = super().kwargs_from_dict(value)
    kwargs["script"] = value.pop("script", None)
    if path := value.pop("script_path", None):
      kwargs["script_path"] = cli_helper.parse_existing_file_path(
          path, name="script_path")
    return kwargs

  def __init__(self,
               script: Optional[str],
               script_path: Optional[pth.LocalPath],
               timeout: dt.timedelta = ACTION_TIMEOUT) -> None:
    self._script = ""
    if bool(script) == bool(script_path):
      raise ValueError(f"One of {self}.script or {self}.path, but not both, "
                       "have to specified. ")
    if script:
      self._script = script
    elif script_path:
      self._script = script_path.read_text()
      logging.debug("Loading script from %s: %s", script_path, script)
      # TODO: support argument injection into shared file script.
    super().__init__(timeout)

  @property
  def script(self) -> str:
    return self._script

  def run_with(self, run: Run, action_runner: ActionRunner) -> None:
    action_runner.inject_new_document_script(run, self)

  def validate(self) -> None:
    super().validate()
    if not self.script:
      raise ValueError(
          f"{self}.script is missing or the provided script file is empty.")

  def to_json(self) -> JsonDict:
    details = super().to_json()
    details["script"] = self.script
    return details


ACTIONS_TUPLE: Tuple[Type[Action], ...] = (
    ClickAction,
    TapAction,
    GetAction,
    ScrollAction,
    SwipeAction,
    WaitAction,
    WaitForElementAction,
    InjectNewDocumentScriptAction,
)

ACTIONS: Dict[ActionType, Type] = {
    action_cls.TYPE: action_cls for action_cls in ACTIONS_TUPLE
}

assert len(ACTIONS_TUPLE) == len(ACTIONS), "Non unique Action.TYPE present"
