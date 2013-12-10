# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The tab switching measurement.

This measurement opens pages in different tabs. After all the tabs have opened,
it cycles through each tab in sequence, and records a histogram of the time
between when a tab was first requested to be shown, and when it was painted.
"""

import time

from metrics import cpu
from metrics import histogram_util
from telemetry.core import util
from telemetry.page import page_measurement
from telemetry.page import page_runner

# TODO: Revisit this test once multitab support is finalized.

class TabSwitching(page_measurement.PageMeasurement):
  def __init__(self):
    super(TabSwitching, self).__init__()
    self._cpu_metric = None

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        '--enable-stats-collection-bindings'
    ])

  def DidStartBrowser(self, browser):
    self._cpu_metric = cpu.Cpu(browser)

  def CanRunForPage(self, page):
    return not page.page_set.pages.index(page)

  def DidNavigateToPage(self, page, tab):
    for i in xrange(1, len(page.page_set.pages)):
      t = tab.browser.tabs.New()

      # We create a test_stub to be able to call 'navigate_steps' on pages
      test_stub = page_measurement.PageMeasurement()
      page_state = page_runner.PageState()
      page_state.PreparePage(page.page_set.pages[i], t)
      page_state.ImplicitPageNavigation(page.page_set.pages[i], t, test_stub)

    # Start tracking cpu load after all pages have loaded.
    self._cpu_metric.Start(page, tab)

  def MeasurePage(self, page, tab, results):
    """Although this is called MeasurePage, we're actually using this function
    to cycle through each tab that was opened via DidNavigateToPage and
    then record a single histogram for the tab switching metric.
    """
    # Calculate the idle cpu load before any actions are done.
    time.sleep(.5)
    self._cpu_metric.Stop(page, tab)
    self._cpu_metric.AddResults(tab, results,
                                'idle_cpu_utilization')

    histogram_name = 'MPArch.RWH_TabSwitchPaintDuration'
    histogram_type = histogram_util.BROWSER_HISTOGRAM
    first_histogram = histogram_util.GetHistogram(
        histogram_type, histogram_name, tab)
    prev_histogram = first_histogram

    for i in xrange(len(tab.browser.tabs)):
      t = tab.browser.tabs[i]
      t.Activate()
      def _IsDone():
        cur_histogram = histogram_util.GetHistogram(
            histogram_type, histogram_name, tab)
        diff_histogram = histogram_util.SubtractHistogram(
            cur_histogram, prev_histogram)
        return diff_histogram
      util.WaitFor(_IsDone, 30)
      prev_histogram = histogram_util.GetHistogram(
          histogram_type, histogram_name, tab)

    last_histogram = histogram_util.GetHistogram(
        histogram_type, histogram_name, tab)
    diff_histogram = histogram_util.SubtractHistogram(last_histogram,
        first_histogram)

    results.AddSummary(histogram_name, '', diff_histogram,
        data_type='unimportant-histogram')
