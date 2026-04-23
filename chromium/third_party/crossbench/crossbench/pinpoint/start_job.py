# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import json
from typing import TYPE_CHECKING

from crossbench.pinpoint import requests
from crossbench.pinpoint.api import PINPOINT_START_JOB_API_URL
from crossbench.pinpoint.helper import annotate

if TYPE_CHECKING:
  from crossbench.pinpoint.config import PinpointTryJobConfig


def start_job(config: PinpointTryJobConfig) -> None:
  """Starts a new Pinpoint job."""
  with annotate("Starting Pinpoint job"):
    response = requests.post(
        PINPOINT_START_JOB_API_URL, data=config.to_request_dict())
    response.raise_for_status()
  print(json.dumps(response.json(), indent=2))
