# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import datetime as dt
import logging
import time

from crossbench.benchmarks.loading import action as i_action
from crossbench.runner.run import Run

from .base import ActionRunner


class BasicActionRunner(ActionRunner):

  def click(self, run: Run, action: i_action.ClickAction) -> None:
    with run.actions("ClickAction", measure=False) as actions:
      # TODO: support more selector types.
      prefix = "xpath/"
      if action.selector.startswith(prefix):
        xpath: str = action.selector[len(prefix):]
        actions.js(
            """
              let element = document.evaluate(arguments[0], document).iterateNext();
              if (arguments[1]) element.scrollIntoView();
              element.click();
            """,
            arguments=[xpath, action.scroll_into_view])
      else:
        raise NotImplementedError(f"Unsupported selector: {action.selector}")

  def get(self, run: Run, action: i_action.GetAction) -> None:
    # TODO: potentially refactor the timing and loggin out to the base class.
    start_time = time.time()
    expected_end_time = start_time + action.duration.total_seconds()

    with run.actions(f"Get {action.url}", measure=False) as actions:
      actions.show_url(action.url, str(action.target))

      if action.ready_state != i_action.ReadyState.ANY:
        # Make sure we also finish if readyState jumps directly
        # from "loading" to "complete"
        actions.wait_js_condition(
            f"""
              let state = document.readyState;
              return state === '{action.ready_state}' || state === "complete";
            """, 0.2, action.timeout.total_seconds())
        return
      # Wait for the given duration from the start of the action.
      wait_time_seconds = expected_end_time - time.time()
      if wait_time_seconds > 0:
        actions.wait(wait_time_seconds)
      elif action.duration:
        run_duration = dt.timedelta(seconds=time.time() - start_time)
        logging.info("%s took longer (%s) than expected action duration (%s).",
                     action, run_duration, action.duration)

  def scroll(self, run: Run, action: i_action.ScrollAction) -> None:
    time_end = time.time() + action.duration.total_seconds()
    direction = 1 if action.direction == i_action.ScrollDirection.UP else -1
    # TODO: properly support scrolling UP by setting a start position.
    start = 0
    end = direction

    with run.actions("ScrollAction", measure=False) as actions:
      while time.time() < time_end:
        actions.js(f"window.scrollTo({start}, {end});")
        start = end
        end += 100 * direction
