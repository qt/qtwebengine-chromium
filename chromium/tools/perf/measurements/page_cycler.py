# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The page cycler measurement.

This measurement registers a window load handler in which is forces a layout and
then records the value of performance.now(). This call to now() measures the
time from navigationStart (immediately after the previous page's beforeunload
event) until after the layout in the page's load event. In addition, two garbage
collections are performed in between the page loads (in the beforeunload event).
This extra garbage collection time is not included in the measurement times.

Finally, various memory and IO statistics are gathered at the very end of
cycling all pages.
"""

import collections
import os
import sys

from metrics import io
from metrics import memory
from metrics import v8_object_stats
from telemetry.core import util
from telemetry.page import page_measurement

class PageCycler(page_measurement.PageMeasurement):
  def __init__(self, *args, **kwargs):
    super(PageCycler, self).__init__(*args, **kwargs)

    with open(os.path.join(os.path.dirname(__file__),
                           'page_cycler.js'), 'r') as f:
      self._page_cycler_js = f.read()

    self._record_v8_object_stats = False

    self._memory_metric = None
    self._v8_object_stats_metric = None
    self._number_warm_runs = None
    self._cold_runs_requested = False
    self._has_loaded_page = collections.defaultdict(int)

  def AddCommandLineOptions(self, parser):
    # The page cyclers should default to 10 iterations. In order to change the
    # default of an option, we must remove and re-add it.
    # TODO: Remove this after transition to run_benchmark.
    pageset_repeat_option = parser.get_option('--pageset-repeat')
    pageset_repeat_option.default = 10
    parser.remove_option('--pageset-repeat')
    parser.add_option(pageset_repeat_option)

    parser.add_option('--v8-object-stats',
        action='store_true',
        help='Enable detailed V8 object statistics.')

    parser.add_option('--cold-load-percent', type='int', default=0,
                      help='%d of page visits for which a cold load is forced')


  def DidStartBrowser(self, browser):
    """Initialize metrics once right after the browser has been launched."""
    self._memory_metric = memory.MemoryMetric(browser)
    if self._record_v8_object_stats:
      self._v8_object_stats_metric = v8_object_stats.V8ObjectStatsMetric()

  def DidStartHTTPServer(self, tab):
    # Avoid paying for a cross-renderer navigation on the first page on legacy
    # page cyclers which use the filesystem.
    tab.Navigate(tab.browser.http_server.UrlOf('nonexistent.html'))

  def WillNavigateToPage(self, page, tab):
    page.script_to_evaluate_on_commit = self._page_cycler_js
    if self.ShouldRunCold(page.url):
      tab.ClearCache()

  def DidNavigateToPage(self, page, tab):
    self._memory_metric.Start(page, tab)
    if self._record_v8_object_stats:
      self._v8_object_stats_metric.Start(page, tab)

  def CustomizeBrowserOptions(self, options):
    memory.MemoryMetric.CustomizeBrowserOptions(options)
    io.IOMetric.CustomizeBrowserOptions(options)
    options.AppendExtraBrowserArgs('--js-flags=--expose_gc')

    if options.v8_object_stats:
      self._record_v8_object_stats = True
      v8_object_stats.V8ObjectStatsMetric.CustomizeBrowserOptions(options)

    # A disk cache bug causes some page cyclers to hang on mac.
    # TODO(tonyg): Re-enable these tests when crbug.com/268646 is fixed.
    if (sys.platform == 'darwin' and
        (sys.argv[-1].endswith('/intl_hi_ru.json') or
         sys.argv[-1].endswith('/tough_layout_cases.json') or
         sys.argv[-1].endswith('/typical_25.json'))):
      print '%s is currently disabled on mac. Skipping test.' % sys.argv[-1]
      sys.exit(0)

    # Handle requests for cold cache runs
    if (options.cold_load_percent and
        (options.repeat_options.page_repeat_secs or
         options.repeat_options.pageset_repeat_secs)):
      raise Exception('--cold-load-percent is incompatible with timed repeat')

    if (options.cold_load_percent and
        (options.cold_load_percent < 0 or options.cold_load_percent > 100)):
      raise Exception('--cold-load-percent must be in the range [0-100]')

    # TODO(rdsmith): Properly handle interaction of page_repeat with
    # dropping the first run.
    number_warm_pageset_runs = int(
        (int(options.repeat_options.pageset_repeat_iters) - 1) *
        (100 - options.cold_load_percent) / 100)

    # Make sure _number_cold_runs is an integer multiple of page_repeat.
    # Without this, --pageset_shuffle + --page_repeat could lead to
    # assertion failures on _started_warm in WillNavigateToPage.
    self._number_warm_runs = (number_warm_pageset_runs *
                              options.repeat_options.page_repeat_iters)
    self._cold_runs_requested = bool(options.cold_load_percent)
    self.discard_first_result = (bool(options.cold_load_percent) or
                                 self.discard_first_result)

  def MeasurePage(self, page, tab, results):
    def _IsDone():
      return bool(tab.EvaluateJavaScript('__pc_load_time'))
    util.WaitFor(_IsDone, 60)
    chart_name = ('times' if not self._cold_runs_requested else
                  'cold_times' if self.ShouldRunCold(page.url) else
                  'warm_times')

    results.Add('page_load_time', 'ms',
                int(float(tab.EvaluateJavaScript('__pc_load_time'))),
                chart_name=chart_name)

    self._has_loaded_page[page.url] += 1

    self._memory_metric.Stop(page, tab)
    self._memory_metric.AddResults(tab, results)
    if self._record_v8_object_stats:
      self._v8_object_stats_metric.Stop(page, tab)
      self._v8_object_stats_metric.AddResults(tab, results)

  def DidRunTest(self, tab, results):
    self._memory_metric.AddSummaryResults(results)
    io.IOMetric().AddSummaryResults(tab, results)

  def ShouldRunCold(self, url):
    # We do the warm runs first for two reasons.  The first is so we can
    # preserve any initial profile cache for as long as possible.
    # The second is that, if we did cold runs first, we'd have a transition
    # page set during which we wanted the run for each URL to both
    # contribute to the cold data and warm the catch for the following
    # warm run, and clearing the cache before the load of the following
    # URL would eliminate the intended warmup for the previous URL.
    return self._has_loaded_page[url] >= self._number_warm_runs + 1

  def results_are_the_same_on_every_page(self):
    return not self._cold_runs_requested
