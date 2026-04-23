# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import difflib
from typing import TYPE_CHECKING, Any, Callable, Iterable, Optional, \
    Protocol, TypeVar

if TYPE_CHECKING:
  from crossbench.path import AnyPath

  InputT = TypeVar("InputT")
  KeyT = TypeVar("KeyT")
  ValueT = TypeVar("ValueT")
  PathT = TypeVar("PathT", bound=AnyPath)

  class GroupTProtocol(Protocol):

    def append(self, item: Any, /) -> None:
      pass

  GroupT = TypeVar("GroupT", bound=GroupTProtocol)


def default_value(value: InputT) -> InputT:
  return value


def group_by(
    collection: Iterable[InputT],
    *,
    key: Callable[[InputT], KeyT],
    value: Callable[[InputT], ValueT] | None = None,
    sort_key: Optional[Callable[[tuple[KeyT, Any]], Any]] = str
) -> dict[KeyT, list[ValueT]]:
  """
  Works similar to itertools.groupby but does a global, SQL-style grouping
  instead of a line-by-line basis like uniq.

  key:      a function that returns the grouping key for a group.
  value:    a function that maps each collection item to a value
            stored in groups.
  sort_key: an optional function that is passed to sorted(..., key=sort_key)
            for sorting the groups.
  """
  return _group_by(
      collection,
      key_fn=key,
      value_fn=value or default_value,
      group_fn=lambda key: [],
      sort_key_fn=sort_key)


def group_by_custom(
    collection: Iterable[InputT],
    *,
    key: Callable[[InputT], KeyT],
    group: Callable[[KeyT], GroupT],
    value: Optional[Callable[[InputT], ValueT]] = None,
    sort_key: Optional[Callable[[tuple[KeyT, Any]], Any]] = str
) -> dict[KeyT, GroupT]:
  """
  Works similar to itertools.groupby but does a global, SQL-style grouping
  instead of a line-by-line basis like uniq.

  key:   a function that returns the grouping key for a group
  group: a function that accepts a group_key and returns a group object that
    has an append() method.
  value: a function that maps each collection item to a value stored in groups.
  sort_key: an optional function that is passed to sorted(..., key=sort_key)
            for sorting the groups.

  Note group_by_custom is only required to make the type system happy.
  """
  return _group_by(
      collection,
      key_fn=key,
      group_fn=group,
      value_fn=value or default_value,
      sort_key_fn=sort_key)


def _group_by(
    collection: Iterable[InputT],
    key_fn: Callable[[InputT], KeyT],
    group_fn: Callable[[KeyT], GroupT],
    value_fn: Callable[[InputT], ValueT],
    sort_key_fn: Optional[Callable[[tuple[KeyT, Any]], Any]] = str
) -> dict[KeyT, GroupT]:
  if not key_fn:  # type: ignore
    raise ValueError("No key function provided")
  groups: dict[KeyT, GroupT] = {}

  for input_item in collection:
    group_key: KeyT = key_fn(input_item)
    group_item: ValueT = value_fn(input_item)
    if selected_group := groups.get(group_key):
      selected_group.append(group_item)
    else:
      new_group: GroupT = group_fn(group_key)
      groups[group_key] = new_group
      new_group.append(group_item)

  if sort_key_fn:
    # sort keys as well for more predictable behavior
    return dict(sorted(groups.items(), key=sort_key_fn))
  return dict(groups.items())


def close_matches_message(choice: str,
                          choices: Iterable[str],
                          name: str = "") -> tuple[str, str | None]:
  choices = tuple(choices)
  if not choices:
    raise ValueError("Expected non-empty choices.")
  similar_choices = difflib.get_close_matches(choice, choices)
  error_message: str = ""
  if name:
    error_message = f"Invalid {name}: {repr(choice)}."
  alternative: str | None = None
  if len(similar_choices) > 1:
    error_message += f" Did you mean one of {', '.join(similar_choices)}?"
  elif len(similar_choices) == 1:
    alternative = similar_choices[0]
    error_message += f" Did you mean {repr(alternative)}?"
  else:
    error_message += f" Choices are {','.join(choices)}"
  return error_message, alternative
