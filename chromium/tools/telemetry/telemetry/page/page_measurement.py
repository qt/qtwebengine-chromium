# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page_test


class MeasurementFailure(page_test.Failure):
  """Exception that can be thrown from MeasurePage to indicate an undesired but
  designed-for problem."""
  pass


class PageMeasurement(page_test.PageTest):
  """Glue code for running a measurement across a set of pages.

  To use this, subclass from the measurement and override MeasurePage. For
  example:

     class BodyChildElementMeasurement(PageMeasurement):
        def MeasurePage(self, page, tab, results):
           body_child_count = tab.EvaluateJavaScript(
               'document.body.children.length')
           results.Add('body_children', 'count', body_child_count)

     if __name__ == '__main__':
         page_measurement.Main(BodyChildElementMeasurement())

  To add test-specific options:

     class BodyChildElementMeasurement(PageMeasurement):
        def AddCommandLineOptions(parser):
           parser.add_option('--element', action='store', default='body')

        def MeasurePage(self, page, tab, results):
           body_child_count = tab.EvaluateJavaScript(
              'document.querySelector('%s').children.length')
           results.Add('children', 'count', child_count)
  """
  def __init__(self,
               action_name_to_run='',
               needs_browser_restart_after_each_run=False,
               discard_first_result=False,
               clear_cache_before_each_run=False):
    super(PageMeasurement, self).__init__(
      '_RunTest',
      action_name_to_run,
      needs_browser_restart_after_each_run,
      discard_first_result,
      clear_cache_before_each_run)

  def _RunTest(self, page, tab, results):
    results.WillMeasurePage(page)
    self.MeasurePage(page, tab, results)
    results.DidMeasurePage()

  @property
  def results_are_the_same_on_every_page(self):
    """By default, measurements are assumed to output the same values for every
    page. This allows incremental output, for example in CSV. If, however, the
    measurement discovers what values it can report as it goes, and those values
    may vary from page to page, you need to override this function and return
    False. Output will not appear in this mode until the entire pageset has
    run."""
    return True

  def MeasurePage(self, page, tab, results):
    """Override to actually measure the page's performance.

    page is a page_set.Page
    tab is an instance of telemetry.core.Tab

    Should call results.Add(name, units, value) for each result, or raise an
    exception on failure. The name and units of each Add() call must be
    the same across all iterations. The name 'url' must not be used.

    Prefer field names that are in accordance with python variable style. E.g.
    field_name.

    Put together:

       def MeasurePage(self, page, tab, results):
         res = tab.EvaluateJavaScript('2+2')
         if res != 4:
           raise Exception('Oh, wow.')
         results.Add('two_plus_two', 'count', res)
    """
    raise NotImplementedError()
