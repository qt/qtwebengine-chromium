# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from crossbench.pinpoint import requests
from crossbench.pinpoint.api import PINPOINT_CONFIG_API_URL
from crossbench.pinpoint.helper import annotate


def fetch_bots() -> list[str]:
  """Fetches the list of available Pinpoint bots from the Pinpoint API."""
  with annotate("Fetching available bots"):
    response = requests.post(PINPOINT_CONFIG_API_URL)
    response.raise_for_status()
    return response.json()["configurations"]
