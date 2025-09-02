# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import contextlib
import datetime as dt
import logging
import sys
import threading
from typing import TYPE_CHECKING, Iterator, Optional, Tuple, Type

import colorama

if TYPE_CHECKING:
  from types import TracebackType

colorama.init()

COLOR_LOGGING: bool = True


class ColoredLogFormatter(logging.Formatter):

  FORMAT = "%(message)s"

  FORMATS = {
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

  def formatException(
      self,
      ei: Tuple[Type[BaseException], BaseException, Optional[TracebackType]]
      | Tuple[None, ...]
  ) -> str:
    return ""

  def formatStack(self, stack_info: str) -> str:
    return ""


@contextlib.contextmanager
def timer(msg: str = "Elapsed Time") -> Iterator[None]:
  start_time = dt.datetime.now()

  def print_timer():
    delta = dt.datetime.now() - start_time
    indent = colorama.Cursor.FORWARD() * 3
    sys.stdout.write(f"{indent}{msg}: {delta}\r")

  with RepeatTimer(interval=0.25, function=print_timer):
    yield


class RepeatTimer(threading.Timer):

  def run(self) -> None:
    while not self.finished.wait(self.interval):
      self.function(*self.args, **self.kwargs)

  def __enter__(self, *args, **kwargs) -> None:
    self.start()

  def __exit__(self, *args, **kwargs) -> None:
    self.cancel()
