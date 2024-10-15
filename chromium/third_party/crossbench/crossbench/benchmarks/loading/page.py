# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import abc
import datetime as dt
from typing import TYPE_CHECKING, Optional, Sequence, Tuple

from crossbench.benchmarks.loading.action import Action
from crossbench.benchmarks.loading.action_runner.base import ActionRunner
from crossbench.benchmarks.loading.action_runner.basic_action_runner import \
    BasicActionRunner
from crossbench.benchmarks.loading.playback_controller import \
    PlaybackController
from crossbench.stories.story import Story

if TYPE_CHECKING:
  from crossbench.runner.run import Run
  from crossbench.types import JsonDict

DEFAULT_DURATION_SECONDS = 15
DEFAULT_DURATION = dt.timedelta(seconds=DEFAULT_DURATION_SECONDS)

class Page(Story, metaclass=abc.ABCMeta):

  url: Optional[str]

  @classmethod
  def all_story_names(cls) -> Tuple[str, ...]:
    return tuple(page.name for page in PAGE_LIST)

  def __init__(self,
               name: str,
               duration: dt.timedelta = DEFAULT_DURATION,
               playback: PlaybackController = PlaybackController.default(),
               about_blank_duration: dt.timedelta = dt.timedelta()):
    self._playback: PlaybackController = playback
    self._about_blank_duration = about_blank_duration
    super().__init__(name, duration)

  def set_parent(self, parent: Page) -> None:
    # TODO: support nested playback controllers.
    self._playback = PlaybackController.default()
    del parent

  def _maybe_navigate_to_about_blank(self, run: Run) -> None:
    if duration := self._about_blank_duration:
      run.browser.show_url(run.runner, "about:blank")
      run.runner.wait(duration)


class LivePage(Page):
  url: str

  def __init__(
      self,
      name: str,
      url: str,
      duration: dt.timedelta = DEFAULT_DURATION,
      playback: PlaybackController = PlaybackController.default(),
      about_blank_duration: dt.timedelta = dt.timedelta()
  ) -> None:
    super().__init__(name, duration, playback, about_blank_duration)
    assert url, "Invalid page url"
    self.url: str = url

  def set_duration(self, duration: dt.timedelta) -> None:
    self._duration = duration

  def details_json(self) -> JsonDict:
    result = super().details_json()
    result["url"] = str(self.url)
    return result

  def run(self, run: Run) -> None:
    for _ in self._playback:
      run.browser.show_url(run.runner, self.url)
      run.runner.wait(self.duration)
      self._maybe_navigate_to_about_blank(run)

  def __str__(self) -> str:
    return f"Page(name={self.name}, url={self.url})"


class CombinedPage(Page):

  def __init__(self,
               pages: Sequence[Page],
               name: str = "combined",
               playback: PlaybackController = PlaybackController.default(),
               about_blank_duration: dt.timedelta = dt.timedelta()):
    assert len(pages), "No sub-pages provided for CombinedPage"
    assert len(pages) > 1, "Combined Page needs more than one page"
    self._pages = pages
    duration = dt.timedelta()
    for page in self._pages:
      page.set_parent(self)
      duration += page.duration
    super().__init__(name, duration, playback, about_blank_duration)
    self.url = None

  def details_json(self) -> JsonDict:
    result = super().details_json()
    result["pages"] = list(page.details_json() for page in self._pages)
    return result

  def run(self, run: Run) -> None:
    for _ in self._playback:
      for page in self._pages:
        page.run(run)

  def __str__(self) -> str:
    combined_name = ",".join(page.name for page in self._pages)
    return f"CombinedPage({combined_name})"


class InteractivePage(Page):

  def __init__(self,
               actions: Tuple[Action, ...],
               name: str,
               playback: PlaybackController = PlaybackController.default(),
               action_runner: Optional[ActionRunner] = None,
               about_blank_duration: dt.timedelta = dt.timedelta()):
    self._name: str = name
    self._action_runner: ActionRunner = action_runner or BasicActionRunner()
    assert isinstance(actions, tuple)
    self._actions: Tuple[Action, ...] = actions
    assert self._actions, "Must have at least 1 valid action"
    duration = self._get_duration()
    super().__init__(name, duration, playback, about_blank_duration)

  @property
  def actions(self) -> Tuple[Action, ...]:
    return self._actions

  @property
  def action_runner(self) -> ActionRunner:
    return self._action_runner

  @action_runner.setter
  def action_runner(self, action_runner: ActionRunner) -> None:
    assert isinstance(self._action_runner, BasicActionRunner)
    self._action_runner = action_runner

  def run(self, run: Run) -> None:
    for _ in self._playback:
      self.action_runner.run_all(run, self._actions)

  def details_json(self) -> JsonDict:
    result = super().details_json()
    result["actions"] = list(action.to_json() for action in self._actions)
    return result

  def _get_duration(self) -> dt.timedelta:
    duration = dt.timedelta()
    for action in self._actions:
      if action.duration is not None:
        duration += action.duration
    return duration


PAGE_LIST = (
    LivePage("blank", "about:blank", dt.timedelta(seconds=1)),
    LivePage("amazon", "https://www.amazon.de/s?k=heizkissen",
             dt.timedelta(seconds=5)),
    LivePage("bing", "https://www.bing.com/images/search?q=not+a+squirrel",
             dt.timedelta(seconds=5)),
    LivePage("caf", "http://www.caf.fr", dt.timedelta(seconds=6)),
    LivePage("cnn", "https://cnn.com/", dt.timedelta(seconds=7)),
    LivePage("ecma262", "https://tc39.es/ecma262/#sec-numbers-and-dates",
             dt.timedelta(seconds=10)),
    LivePage("expedia", "https://www.expedia.com/", dt.timedelta(seconds=7)),
    LivePage("facebook", "https://facebook.com/shakira",
             dt.timedelta(seconds=8)),
    LivePage("maps", "https://goo.gl/maps/TEZde4y4Hc6r2oNN8",
             dt.timedelta(seconds=10)),
    LivePage("microsoft", "https://microsoft.com/", dt.timedelta(seconds=6)),
    LivePage("provincial", "http://www.provincial.com",
             dt.timedelta(seconds=6)),
    LivePage("sueddeutsche", "https://www.sueddeutsche.de/wirtschaft",
             dt.timedelta(seconds=8)),
    LivePage("theverge", "https://www.theverge.com/", dt.timedelta(seconds=10)),
    LivePage("timesofindia", "https://timesofindia.indiatimes.com/",
             dt.timedelta(seconds=8)),
    LivePage("twitter", "https://twitter.com/wernertwertzog?lang=en",
             dt.timedelta(seconds=6)),
)
PAGES = {page.name: page for page in PAGE_LIST}
PAGE_LIST_SMALL = (PAGES["facebook"], PAGES["maps"], PAGES["timesofindia"],
                   PAGES["cnn"])
