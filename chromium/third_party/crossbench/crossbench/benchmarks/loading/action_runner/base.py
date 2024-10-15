# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import abc
from typing import TYPE_CHECKING, Iterable

if TYPE_CHECKING:
  from crossbench.benchmarks.loading import action as i_action
  from crossbench.runner.run import Run


class ActionRunner(abc.ABC):

  def run_all(self, run: Run, actions: Iterable[i_action.Action]):
    for action in actions:
      action.run_with(run, self)

  def wait(self, run: Run, action: i_action.WaitAction) -> None:
    with run.actions("WaitAction", measure=False) as actions:
      actions.wait(action.duration)

  @abc.abstractmethod
  def scroll(self, run: Run, action: i_action.ScrollAction) -> None:
    pass

  @abc.abstractmethod
  def get(self, run: Run, action: i_action.GetAction) -> None:
    pass

  @abc.abstractmethod
  def click(self, run: Run, action: i_action.ClickAction) -> None:
    pass

  @abc.abstractmethod
  def tap(self, run: Run, action: i_action.TapAction) -> None:
    pass

  @abc.abstractmethod
  def swipe(self, run: Run, action: i_action.SwipeAction) -> None:
    pass

  @abc.abstractmethod
  def wait_for_element(self, run: Run,
                       action: i_action.WaitForElementAction) -> None:
    pass

  @abc.abstractmethod
  def inject_new_document_script(
      self, run: Run, action: i_action.InjectNewDocumentScriptAction) -> None:
    pass
