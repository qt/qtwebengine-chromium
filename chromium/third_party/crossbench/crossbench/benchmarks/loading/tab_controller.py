# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import abc
import dataclasses
from typing import Iterator

from typing_extensions import override

from crossbench.config import ConfigObject
from crossbench.parse import NumberParser


class TabController(ConfigObject):

  @classmethod
  @override
  def parse_str(cls, value: str) -> TabController:
    if not value or value == "single":
      return cls.single()
    if value in ("inf", "infinity"):
      return cls.forever()
    loops = NumberParser.positive_int(value, "Repeat-count")
    return cls.repeat(loops)

  @classmethod
  def default(cls) -> TabController:
    return cls.single()

  @classmethod
  def single(cls) -> TabController:
    return SingleTabController()

  @classmethod
  def multiple(cls) -> TabController:
    return RepeatTabController(1)

  @classmethod
  def repeat(cls, count: int) -> RepeatTabController:
    return RepeatTabController(count)

  @classmethod
  def forever(cls) -> TabController:
    return ForeverTabController()

  @abc.abstractmethod
  def __iter__(self) -> Iterator[None]:
    pass

  @property
  def multiple_tabs(self) -> bool:
    return True


@dataclasses.dataclass(frozen=True)
class SingleTabController(TabController):
  """
  Open given urls in one tab sequentially.
  """

  def __iter__(self) -> Iterator[None]:
    yield None

  @property
  @override
  def multiple_tabs(self) -> bool:
    return False


@dataclasses.dataclass(frozen=True)
class ForeverTabController(TabController):
  """
  Open given urls in separate tabs and repeat infinitely until
  one of the tabs gets discarded.

  Example 1: if url='cnn', it keeps opening new tabs loading cnn.

  Example 2: if urls='amazon,cnn', it keeps opening
  amazon,cnn,amazon,cnn,amazon,cnn,.... ....
  """

  def __iter__(self) -> Iterator[None]:
    while True:
      yield None


@dataclasses.dataclass(frozen=True)
class RepeatTabController(TabController):
  """
  Open given urls in separate tabs and repeat for `count` times.

  Example 1: if url='cnn', count=3, it will open 3 tabs: cnn,cnn,cnn.

  Example 2: if urls='amazon,cnn', count=3, it will open 6 tabs:
  amazon,cnn,amazon,cnn,amazon,cnn
  """
  count: int

  def __iter__(self) -> Iterator[None]:
    for _ in range(self.count):
      yield None
