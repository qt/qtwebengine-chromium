# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import enum
import textwrap
from typing import Any, List, NamedTuple, Optional, Self, Tuple, Type, cast

import tabulate


class StrHelpDataMixin(NamedTuple):
  value: str
  help: str



class StrEnumWithHelp(StrHelpDataMixin, enum.Enum):

  @classmethod
  def _missing_(cls: Type[Self], value: Any) -> Optional[Self]:
    value = str(value).lower()
    for member in cls:
      if member.value == value:
        return member
    return None

  @classmethod
  def help_text_items(cls) -> List[Tuple[str, str]]:
    return [
        (repr(instance.value), instance.help) for instance in cls  # pytype: disable=missing-parameter
    ]

  @classmethod
  def help_text(cls, indent: int = 0) -> str:
    text: str = tabulate.tabulate(cls.help_text_items(), tablefmt="plain")
    if indent:
      return textwrap.indent(text, " " * indent)
    return text

  def __str__(self) -> str:
    return cast(str, self.value)
