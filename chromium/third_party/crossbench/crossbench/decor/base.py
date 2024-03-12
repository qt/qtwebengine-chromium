# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import abc
import argparse
import datetime as dt
from typing import Any, Dict, Generic, Optional, Set, Type, TypeVar

from crossbench import compat, plt
from crossbench.config import ConfigParser
from crossbench.probes.results import EmptyProbeResult, ProbeResult

DecoratorT = TypeVar("DecoratorT", bound="Decorator")
DecoratorTargetT = TypeVar("DecoratorTargetT")


class DecoratorConfigParser(ConfigParser[DecoratorT]):

  def __init__(self, probe_cls: Type[DecoratorT]) -> None:
    super().__init__(probe_cls.__name__, probe_cls)
    self._probe_cls = probe_cls


class Decorator(abc.ABC, Generic[DecoratorTargetT]):
  """ Abstract base class for RunDecorator and SessionDecorator that can
  temporarily modify Runs or BrowserSessions.
  """

  @property
  @abc.abstractmethod
  def NAME(self) -> str:
    pass

  @classmethod
  def config_parser(cls) -> DecoratorConfigParser:
    return DecoratorConfigParser(cls)

  @classmethod
  def from_config(cls: Type[DecoratorT], config_data: Dict) -> DecoratorT:
    config_parser = cls.config_parser()
    kwargs: Dict[str, Any] = config_parser.kwargs_from_config(config_data)
    if config_data:
      raise argparse.ArgumentTypeError(
          f"Config for {cls.__name__}={cls.NAME} contains unused properties: "
          f"{', '.join(config_data.keys())}")
    return cls(**kwargs)

  @classmethod
  def help_text(cls) -> str:
    return str(cls.config_parser())

  def __init__(self) -> None:
    assert self.name is not None, f"{type(self).__name__} must have a name"
    self._targets: Set[DecoratorTargetT] = set()

  def __str__(self) -> str:
    return type(self).__name__

  @property
  def runner_platform(self) -> plt.Platform:
    return plt.PLATFORM

  @property
  def name(self) -> str:
    return self.NAME

  @abc.abstractmethod
  def context(
      self: DecoratorT,
      target: DecoratorTargetT,
  ) -> DecoratorContext[DecoratorT, DecoratorTargetT]:
    pass


class DecoratorContext(abc.ABC, Generic[DecoratorT, DecoratorTargetT]):
  """
  The active python context-manager for a Decorator with a life-time interface
  to manage measurement, services or resources.

  +- setup()
  |  Unmeasured scope, browser might not be active yet.
  |
  | +- start()
  | |  Browser active / measured section.
  | +- stop()
  |
  *- tear_down()
  """

  class _State(compat.StrEnum):
    READY = "ready"
    STARTING = "startup"
    RUNNING = "running"
    SUCCESS = "success"
    FAILURE = "failure"

  def __init__(self, decorator: DecoratorT, target: DecoratorTargetT) -> None:
    self._decorator = decorator
    self._target = target
    self._state = self._State.READY
    self._is_success: bool = False
    self._start_time: Optional[dt.datetime] = None
    self._stop_time: Optional[dt.datetime] = None
    self._label = f"{type(self).__name__} {self.name}"

  @property
  def name(self) -> str:
    return self._decorator.name

  @property
  def label(self) -> str:
    return self._label

  @property
  def start_time(self) -> dt.datetime:
    """
    Returns a unified start time that is the same for all active Decorators.
    This can be used to account for startup delays caused by other Decorators.
    """
    assert self._start_time
    return self._start_time

  @property
  def duration(self) -> dt.timedelta:
    assert self._start_time and self._stop_time
    return self._stop_time - self._start_time

  @property
  def is_success(self) -> bool:
    return self._is_success

  def set_start_time(self, start_datetime: dt.datetime) -> None:
    # Used to set a uniform start time across all active DecoratorContexts.
    assert self._start_time is None
    self._start_time = start_datetime

  def __enter__(self) -> None:
    assert self._state is self._State.READY
    self._state = self._State.STARTING
    with self._target.exception_handler(f"{self._label} start"):
      try:
        self.start()
        self._state = self._State.RUNNING
      except:
        self._state = self._State.FAILURE
        raise

  def __exit__(self, exc_type, exc_value, traceback) -> None:
    assert (self._state is self._State.RUNNING or
            self._state is self._State.FAILURE)
    with self._target.exception_handler(f"{self._label} stop"):
      try:
        self.stop()
        if self._state == self._State.RUNNING:
          self._state = self._State.SUCCESS
      except:
        self._state = self._State.FAILURE
        raise
      finally:
        self._stop_time = dt.datetime.now()

  def setup(self) -> None:
    """
    Called before starting the target.
    Not on the critical path, can be used for heavy computation.
    """

  def start(self) -> None:
    """
    Called immediately before starting the given target, after the browser
    started.
    This method should have as little overhead as possible.
    If possible, delegate heavy computation to the "setup" method.
    """

  def stop(self) -> None:
    """
    Called immediately after finishing the given Target with the browser still
    running.
    This method should have as little overhead as possible.
    If possible, delegate heavy computation to the "tear_down" method.
    """

  def tear_down(self) -> ProbeResult:
    """
    Non time-critical, called after stopping all Decorators and after stopping
    the target.
    Heavy post-processing can be performed here without affect the result of
    other DecoratorContexts.
    """
    return EmptyProbeResult()
