# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from crossbench.pinpoint import requests
from crossbench.pinpoint.api import CHROMEPERF_TEST_SUITES_API_URL
from crossbench.pinpoint.helper import annotate


def fetch_benchmarks() -> list[str]:
  """Fetches the list of available benchmarks from the Chromeperf API."""
  with annotate("Fetching benchmarks"):
    response = requests.post(CHROMEPERF_TEST_SUITES_API_URL)
    response.raise_for_status()
    return response.json()
