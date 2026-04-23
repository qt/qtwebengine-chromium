# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import contextlib
import datetime as dt
import logging
import sys
import threading
from typing import TYPE_CHECKING, Final, Iterator, Optional, Type

import colorama
from typing_extensions import override

from crossbench.helper import terminal
from crossbench.helper.spinner import Spinner

if TYPE_CHECKING:
  from types import TracebackType

colorama.init()

IS_ATTY: Final[bool] = hasattr(sys.stdout, "isatty") and sys.stdout.isatty()
COLOR_LOGGING: bool = True


class ColoredLogFormatter(logging.Formatter):

  FORMAT: Final = "%(message)s"

  FORMATS: Final = {
      logging.DEBUG:
          FORMAT,
      logging.INFO:
          str(colorama.Fore.GREEN) + FORMAT + str(colorama.Fore.RESET),
      logging.WARNING:
          str(colorama.Fore.YELLOW) + FORMAT + str(colorama.Fore.RESET),
      logging.ERROR:
          str(colorama.Fore.RED) + FORMAT + str(colorama.Fore.RESET),
      logging.CRITICAL:
          str(colorama.Style.BRIGHT) + FORMAT + str(colorama.Style.RESET_ALL),
  }

  def format(self, record: logging.LogRecord) -> str:
    log_fmt = self.FORMATS.get(record.levelno)
    formatter = logging.Formatter(log_fmt)
    return formatter.format(record)

  @override
  def formatException(
      self,
      ei: tuple[Type[BaseException], BaseException, Optional[TracebackType]]
      | tuple[None, ...]
  ) -> str:
    del ei
    return ""

  @override
  def formatStack(self, stack_info: str) -> str:
    del stack_info
    return ""


def format_duration(duration: dt.timedelta) -> str:
  remaining_s = duration.total_seconds()
  hours = remaining_s // 3600
  # remaining seconds
  remaining_s = remaining_s - (hours * 3600)
  # minutes
  minutes = remaining_s // 60
  # remaining seconds
  seconds = remaining_s - (minutes * 60)

  formatted = f"{round(seconds, 1)}s"
  if minutes:
    formatted = f"{minutes}m{formatted}"
  if hours:
    formatted = f"{hours}h{formatted}"
  return formatted


def write_indented(msg: str) -> None:
  indent = colorama.Cursor.FORWARD() * 30
  sys.stdout.write(f"{indent}{msg}\r")


def clear_indented() -> None:
  indent = colorama.Cursor.FORWARD() * 30
  sys.stdout.write(f"{indent}{terminal.CLEAR_END}\r")


DEFAULT_INTERVAL_S: Final[float] = 0.5


@contextlib.contextmanager
def timer(msg: str = "Elapsed Time",
          update_interval: float = DEFAULT_INTERVAL_S) -> Iterator[None]:
  if not IS_ATTY:
    yield
    return

  start_time = dt.datetime.now()

  def print_timer() -> None:
    delta = dt.datetime.now() - start_time
    write_indented(f"{msg}: {format_duration(delta)}")

  try:
    with RepeatTimer(interval=update_interval, function=print_timer):
      yield
  finally:
    clear_indented()


@contextlib.contextmanager
def countdown(duration: dt.timedelta,
              msg: str = "Waiting",
              update_interval: float = DEFAULT_INTERVAL_S) -> Iterator[None]:
  if not IS_ATTY:
    print(f"{msg}: {format_duration(duration)}")
    yield
    return

  start_time = dt.datetime.now()

  def print_timer() -> None:
    delta = dt.datetime.now() - start_time
    time_left = duration - delta
    write_indented(f"{msg}: {format_duration(time_left)}")

  try:
    with RepeatTimer(interval=update_interval, function=print_timer):
      yield
  finally:
    clear_indented()


class RepeatTimer(threading.Timer):

  def run(self) -> None:
    while not self.finished.wait(self.interval):
      self.function(*self.args, **self.kwargs)

  def __enter__(self, *args, **kwargs) -> None:
    self.start()

  def __exit__(self, *args, **kwargs) -> None:
    self.cancel()


@contextlib.contextmanager
def spinner(sleep: float = 0.5, title: str = "") -> Iterator[Spinner]:
  with Spinner(IS_ATTY, sleep, title).open() as spinner_instance:
    yield spinner_instance


def prompt(message: str = "",
           hint: str = "",
           color: str = colorama.Fore.RESET) -> str:
  return input(f"{color}{message}{colorama.Fore.RESET} {hint}")
