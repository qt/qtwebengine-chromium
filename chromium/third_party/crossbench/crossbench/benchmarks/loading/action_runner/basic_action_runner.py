# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import datetime as dt
import logging
import time

from crossbench.benchmarks.loading import action as i_action
from crossbench.benchmarks.loading.action_runner.base import ActionRunner
from crossbench.runner.run import Run


class BasicActionRunner(ActionRunner):
  XPATH_SELECT = """
      let element = document.evaluate(arguments[0], document).iterateNext();
      if (!element) return false;
      if (arguments[1]) element.scrollIntoView();
      element.click();
      return true;
    """
  CSS_SELECT = """
      let element = document.querySelector(arguments[0]);
      if (!element) return false;
      if (arguments[1]) element.scrollIntoView();
      element.click();
      return true;
    """

  def click(self, run: Run, action: i_action.ClickAction) -> None:
    with run.actions("ClickAction", measure=False) as actions:
      # TODO: support more selector types
      prefix = "xpath/"
      selector: str = action.selector
      if selector.startswith(prefix):
        selector = selector[len(prefix):]
        script = self.XPATH_SELECT
      else:
        script = self.CSS_SELECT
      found_element = actions.js(
          script, arguments=[selector, action.scroll_into_view])
      if not found_element and action.required:
        raise RuntimeError(
            f"Could not find matching DMO element: {repr(selector)}")

  def get(self, run: Run, action: i_action.GetAction) -> None:
    # TODO: potentially refactor the timing and logging out to the base class.
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
    with run.actions("ScrollAction", measure=False) as actions:
      duration_s = action.duration.total_seconds()
      distance = action.distance
      initial_scrollY = actions.js("return window.scrollY")
      start_time = time.time()
      # TODO: use the chrome.gpuBenchmarking.smoothScrollBy extension
      # if available.
      while True:
        time_delta = time.time() - start_time
        if time_delta >= duration_s:
          break
        scrollY = initial_scrollY + time_delta / duration_s * distance
        actions.js(f"window.scrollTo({{top:{scrollY}, behavior:'smooth'}});")
        actions.wait(0.2)
      scrollY = initial_scrollY + distance
      actions.js(f"window.scrollTo({{top:{scrollY}, behavior:'smooth'}});")

  def wait_for_element(self, run: Run,
                       action: i_action.WaitForElementAction) -> None:
    with run.actions("WaitForElementAction", measure=False) as actions:
      timeout_ms = action.timeout // dt.timedelta(milliseconds=1)
      result = actions.js(
          """
            const [selector, timeout_ms] = arguments;
            if (document.querySelector(selector)) {
              return true;
            }
            return await new Promise(resolve => {
              const timer = setTimeout(() => {
                resolve(false);
              }, timeout_ms);

              const observer = new MutationObserver(mutations => {
                if (document.querySelector(selector)) {
                  observer.disconnect();
                  clearTimeout(timer);
                  resolve(true);
                }
              });

              observer.observe(document.body, {
                  childList: true,
                  subtree: true
              });
            });
          """,
          arguments=[action.selector, timeout_ms])

      if not result:
        logging.warning("Timed out waiting for '%s'", action.selector)

  def inject_new_document_script(
      self, run: Run, action: i_action.InjectNewDocumentScriptAction) -> None:
    run.browser.run_script_on_new_document(action.script)

  def tap(self, run: Run, action: i_action.TapAction) -> None:
    raise NotImplementedError("Tap action not implemented in BasicActionRunner")

  def swipe(self, run: Run, action: i_action.SwipeAction) -> None:
    raise NotImplementedError(
        "Swipe action not implemented in BasicActionRunner")
