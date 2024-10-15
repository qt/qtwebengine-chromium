# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import argparse
import copy
import dataclasses
import datetime as dt
import json
import logging
from typing import (TYPE_CHECKING, Any, Dict, List, Optional, Sequence, Tuple,
                    Type)
from urllib.parse import urlparse

from crossbench import cli_helper, exception
from crossbench import path as pth
from crossbench.benchmarks.loading.action import (ACTIONS, Action, ActionType,
                                                  ClickAction, GetAction,
                                                  ReadyState, WaitAction)
from crossbench.benchmarks.loading.page import PAGES
from crossbench.benchmarks.loading.playback_controller import \
    PlaybackController
from crossbench.config import ConfigObject

if TYPE_CHECKING:
  from crossbench.types import JsonDict


@dataclasses.dataclass(frozen=True)
class PageConfig(ConfigObject):
  label: str = ""
  url: str = ""
  duration: dt.timedelta = dt.timedelta()
  playback: Optional[PlaybackController] = None
  actions: Tuple[Action, ...] = tuple()

  @classmethod
  def loads(cls: Type[PageConfig], value: str) -> PageConfig:
    parts = value.rsplit(",", maxsplit=1)
    if len(parts) == 1:
      label, url = cls._parse_url(parts[0])
      return PageConfig(label=label, url=url)
    url, duration_str = parts
    label, url = cls._parse_url(url)
    return PageConfig(
        label=label,
        url=url,
        duration=cli_helper.Duration.parse_non_zero(duration_str))

  @classmethod
  def load_dict(cls: Type[PageConfig], config: Dict[str, Any]) -> PageConfig:
    # TODO: use this method and move actions parsing to here from PagesConfig
    url = config['url']
    label, url = cls._parse_url(url)
    duration = dt.timedelta()
    if duration_str := config.get("duration"):
      duration = cli_helper.Duration.parse_non_zero(duration_str)
    return PageConfig(label=label, url=url, duration=duration)

  @classmethod
  def _parse_url(cls, value: str) -> Tuple[str, str]:
    if value in PAGES:
      return value, PAGES[value].url
    url = urlparse(value)
    if not url.scheme:
      value = f"https://{value}"
    return cls._url_extract_label(value), value

  @classmethod
  def _url_extract_label(cls, value: str) -> str:
    url = urlparse(value)
    if url.scheme == "about":
      return url.path
    if url.scheme == "file":
      return pth.LocalPath(url.path).name
    if hostname := url.hostname:
      if hostname.startswith("www."):
        return hostname[len("www."):]
      return hostname
    return value


@dataclasses.dataclass(frozen=True)
class PagesConfig(ConfigObject):
  pages: Tuple[PageConfig, ...] = ()

  def __post_init__(self) -> None:
    super().__post_init__()
    for index, page in enumerate(self.pages):
      assert isinstance(page, PageConfig), (
          f"pages[{index}] is not a PageConfig but {type(page).__name__}")

  @classmethod
  def loads(cls, value: str) -> PagesConfig:
    values: List[str] = []
    previous_part: Optional[str] = None
    for part in value.strip().split(","):
      part = cli_helper.parse_non_empty_str(part, "url or duration")
      try:
        cli_helper.Duration.parse_non_zero(part)
        if not previous_part:
          raise argparse.ArgumentTypeError(
              "Duration can only follow after url. "
              f"Current value: {repr(part)}")
        values[-1] = f"{previous_part},{part}"
        previous_part = None
      except cli_helper.DurationParseError:
        previous_part = part
        values.append(part)
    return cls.load_sequence(values)

  @classmethod
  def parse_other(cls, value: Any) -> PagesConfig:
    if isinstance(value, (list, tuple)):
      return cls.load_sequence(value)
    return super().parse_other(value)

  @classmethod
  def load_sequence(cls, values: Sequence[str]) -> PagesConfig:
    if not values:
      raise argparse.ArgumentTypeError("Got empty page list.")
    pages: List[PageConfig] = []
    for index, single_line_config in enumerate(values):
      with exception.annotate_argparsing(
          f"Parsing pages[{index}]: {repr(single_line_config)}"):
        pages.append(PageConfig.parse(single_line_config))
    return PagesConfig(pages=tuple(pages))

  @classmethod
  def load_dict(cls, config: Dict) -> PagesConfig:
    with exception.annotate_argparsing("Parsing scenarios / pages"):
      if "pages" not in config:
        raise argparse.ArgumentTypeError(
            "Config does not provide a 'pages' dict.")
      pages = cli_helper.parse_non_empty_dict(config["pages"], "pages")
      with exception.annotate_argparsing("Parsing config 'pages'"):
        pages = copy.deepcopy(pages)
        return cls._parse_pages(pages)
    raise exception.UnreachableError()

  @classmethod
  def _parse_pages(cls, data: Dict[str, Any]) -> PagesConfig:
    """
    Behaviour to be aware

    There's no default Actions. In other words, if there's no Actions
    for a scenario this specific scenario will be ignored since there's
    nothing to do.

    If one would want to simply navigate to a site it is important to at
    least include: {action: "GET", value/url: google.com} in the specific
    scenario.

    As an example look at: config/doc/page.config.hjson
    """
    pages = []
    for scenario_name, actions in data.items():
      with exception.annotate_argparsing(
          f"Parsing scenario ...['{scenario_name}']"):
        actions = cls._parse_actions(actions, scenario_name)
        url = cls._extract_first_actions_url(actions)
        pages.append(
            PageConfig(
                label=scenario_name, url=url, playback=None, actions=actions))
    return PagesConfig(tuple(pages))

  @classmethod
  def _parse_actions(cls, actions: List[Dict[str, Any]],
                     scenario_name: str) -> Tuple[Action, ...]:
    if not actions:
      raise ValueError(f"Scenario '{scenario_name}' has no action")
    if not isinstance(actions, list):
      raise ValueError(f"Expected list, got={type(actions)}, '{actions}'")
    actions_list: List[Action] = []
    for i, action_config in enumerate(actions):
      with exception.annotate_argparsing(
          f"Parsing action   ...['{scenario_name}'][{i}]"):
        action_step = cls._parse_action(i, action_config)
        actions_list.append(action_step)
    if not actions_list:
      raise argparse.ArgumentTypeError(
          f"Expect non-empty actions for {scenario_name}")
    return tuple(actions_list)

  @classmethod
  def _parse_action(cls, i, action_config: JsonDict) -> Action:
    if "action" not in action_config:
      raise argparse.ArgumentTypeError(
          f"Missing 'action' property in {json.dumps(action_config)}")
    action_type: ActionType = ActionType.parse(action_config.get("action"))
    action_cls: Type[Action] = ACTIONS[action_type]
    with exception.annotate_argparsing(
        f"Parsing details  ...[{i}]{{ action: \"{action_type}\", ...}}:"):
      kwargs = action_cls.kwargs_from_dict(action_config)
      return action_cls(**kwargs)
    raise exception.UnreachableError()

  @classmethod
  def _extract_first_actions_url(cls, actions: Sequence[Action]) -> str:
    for action in actions:
      if isinstance(action, GetAction):
        return action.url
    raise argparse.ArgumentTypeError("Actions must contain at least one GET.")


class DevToolsRecorderPagesConfig(PagesConfig):

  @classmethod
  def loads(cls: Type[PagesConfig], value: str) -> PagesConfig:
    raise NotImplementedError()

  @classmethod
  def load_dict(cls, config: Dict[str, Any]) -> DevToolsRecorderPagesConfig:
    config = cli_helper.parse_non_empty_dict(config)
    with exception.annotate_argparsing("Loading DevTools recording file"):
      title = config["title"]
      assert title, "No title provided"
      actions = tuple(cls._parse_steps(config["steps"]))
      url = cls._extract_first_actions_url(actions)
      pages = (PageConfig(label=title, url=url, actions=actions),)
      return DevToolsRecorderPagesConfig(pages)
    raise exception.UnreachableError()

  @classmethod
  def _parse_steps(cls, steps: List[Dict[str, Any]]) -> List[Action]:
    actions: List[Action] = []
    for step in steps:
      maybe_actions: Optional[Action] = cls._parse_step(step)
      if maybe_actions:
        actions.append(maybe_actions)
        # TODO(cbruni): make this configurable
        actions.append(WaitAction(duration=dt.timedelta(seconds=1)))
    return actions

  @classmethod
  def _parse_step(cls, step: Dict[str, Any]) -> Optional[Action]:
    step_type: str = step["type"]
    default_timeout = dt.timedelta(seconds=10)
    if step_type == "navigate":
      return GetAction(  # type: ignore
          step["url"], ready_state=ReadyState.COMPLETE)
    if step_type == "click":
      selectors: List[List[str]] = step["selectors"]
      xpath: Optional[str] = None
      for selector_list in selectors:
        for selector in selector_list:
          if selector.startswith("xpath//"):
            xpath = selector
            break
      assert xpath, "Need xpath selector for click action"
      return ClickAction(xpath, scroll_into_view=True, timeout=default_timeout)
    if step_type == "setViewport":
      # Resizing is ignored for now.
      return None
    raise ValueError(f"Unsupported step: {step_type}")


class ListPagesConfig(PagesConfig):

  VALID_EXTENSIONS: Tuple[str, ...] = (".txt", ".list")

  @classmethod
  def loads(cls, value: str) -> PagesConfig:
    raise argparse.ArgumentTypeError(
        f"URL list file {repr(value)} does not exist.")

  @classmethod
  def load_path(cls, path: pth.LocalPath) -> PagesConfig:
    pages: List[PageConfig] = []
    with exception.annotate_argparsing(f"Loading Pages list file: {path.name}"):
      line: int = 0
      with path.open() as f:
        for single_line_config in f.readlines():
          with exception.annotate_argparsing(f"Parsing line {line}"):
            line += 1
            single_line_config = single_line_config.strip()
            if not single_line_config:
              logging.warning("Skipping empty line %s", line)
              continue
            pages.append(PageConfig.parse(single_line_config))
    return PagesConfig(pages=tuple(pages))

  @classmethod
  def load_dict(cls, config: Dict) -> PagesConfig:
    config = cli_helper.parse_non_empty_dict(config, "pages")
    with exception.annotate_argparsing("Parsing scenarios / pages"):
      if "pages" not in config:
        raise argparse.ArgumentTypeError(
            "Config does not provide a 'pages' dict.")
      pages = config['pages']
      if isinstance(pages, str):
        pages = [pages]
      if not isinstance(pages, (list, tuple)):
        raise argparse.ArgumentTypeError(
            f"Expected list/tuple for pages, but got {type(pages)}")
      return cls.load_sequence(pages)
    raise exception.UnreachableError()
