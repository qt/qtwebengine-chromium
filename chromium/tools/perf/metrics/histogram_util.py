# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This is a helper module to get and manipulate histogram data.

The histogram data is the same data as is visible from "chrome://histograms".
More information can be found at: chromium/src/base/metrics/histogram.h
"""

import json
import logging

BROWSER_HISTOGRAM = 'browser_histogram'
RENDERER_HISTOGRAM = 'renderer_histogram'

def SubtractHistogram(histogram_json, start_histogram_json):
  """Subtracts a previous histogram from a histogram.

  Both parameters and the returned result are json serializations.
  """
  start_histogram = json.loads(start_histogram_json)
  # It's ok if the start histogram is empty (we had no data, maybe even no
  # histogram at all, at the start of the test).
  if 'buckets' not in start_histogram:
    return histogram_json

  histogram = json.loads(histogram_json)
  if ('pid' in start_histogram and 'pid' in histogram
      and start_histogram['pid'] != histogram['pid']):
    raise Exception(
        'Trying to compare histograms from different processes (%d and %d)'
        % (start_histogram['pid'], histogram['pid']))

  start_histogram_buckets = dict()
  for b in start_histogram['buckets']:
    start_histogram_buckets[b['low']] = b['count']

  new_buckets = []
  for b in histogram['buckets']:
    new_bucket = b
    low = b['low']
    if low in start_histogram_buckets:
      new_bucket['count'] = b['count'] - start_histogram_buckets[low]
      if new_bucket['count'] < 0:
        logging.error('Histogram subtraction error, starting histogram most '
                      'probably invalid.')
    if new_bucket['count']:
      new_buckets.append(new_bucket)
  histogram['buckets'] = new_buckets
  histogram['count'] -= start_histogram['count']

  return json.dumps(histogram)


def GetHistogram(histogram_type, histogram_name, tab):
  """Get a json serialization of a histogram."""
  assert histogram_type in [BROWSER_HISTOGRAM, RENDERER_HISTOGRAM]
  function = 'getHistogram'
  if histogram_type == BROWSER_HISTOGRAM:
    function = 'getBrowserHistogram'
  histogram_json = tab.EvaluateJavaScript(
      'statsCollectionController.%s("%s")' %
      (function, histogram_name))
  if histogram_json:
    return histogram_json
  return None
