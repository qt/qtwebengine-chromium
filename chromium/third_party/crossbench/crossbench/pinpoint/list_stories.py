# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from crossbench.pinpoint import requests
from crossbench.pinpoint.api import CHROMEPERF_DESCRIBE_API_URL
from crossbench.pinpoint.helper import annotate


def fetch_stories(benchmark_name: str) -> list[str]:
  """Fetches the list of available stories for a given benchmark."""
  params = {"test_suite": benchmark_name, "master": "ChromiumPerf"}
  with annotate(f"Fetching stories for benchmark '{benchmark_name}'"):
    response = requests.post(CHROMEPERF_DESCRIBE_API_URL, params=params)
    response.raise_for_status()
    data = response.json()
    return data.get("cases", [])
