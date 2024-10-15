# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import datetime as dt
import logging
import time

from crossbench.benchmarks.loading import action as i_action
from crossbench.benchmarks.loading.action_runner.basic_action_runner import \
    BasicActionRunner
from crossbench.runner.run import Run


class AndroidInputActionRunner(BasicActionRunner):

  def tap(self, run: Run, action: i_action.TapAction) -> None:
    with run.actions("TapAction", measure=False) as actions:
      if action.selector:
        x, y = actions.js(
            """
              const rect =
                  document.querySelector(arguments[0]).getBoundingClientRect();
              const ratio = window.devicePixelRatio;
              // Assume all the difference between window and screen positions
              // comes from the top (e.g. toolbar). This is not exactly true.
              // TODO: find a better way to calculate offset.
              const offsetY = screen.height - window.innerHeight;
              return [ratio * (rect.left + rect.width / 2),
                      ratio * (offsetY + rect.top + rect.height / 2)];
            """,
            arguments=[action.selector])
      else:
        x = action.x
        y = action.y

      run.browser.platform.sh('input', 'tap', str(x), str(y))

  def swipe(self, run: Run, action: i_action.SwipeAction) -> None:
    with run.actions("SwipeAction", measure=False):
      x1 = str(action.startx)
      y1 = str(action.starty)
      x2 = str(action.endx)
      y2 = str(action.endy)
      dur_ms = str(action.duration // dt.timedelta(milliseconds=1))
      run.browser.platform.sh('input', 'swipe', x1, y1, x2, y2, dur_ms)
