# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import json
from typing import Any

from crossbench.pinpoint import requests
from crossbench.pinpoint.api import PINPOINT_JOB_API_URL_TEMPLATE
from crossbench.pinpoint.config import PinpointTryJobConfig
from crossbench.pinpoint.helper import annotate


def fetch_job_config(job_id: str, full: bool = False) -> dict[str, Any]:
  """Fetches the configuration for a specific Pinpoint job."""
  url = PINPOINT_JOB_API_URL_TEMPLATE.format(job_id=job_id)
  params = {}
  if full:
    params["o"] = ["STATE", "ESTIMATE"]
  with annotate("Fetching Job Config"):
    response = requests.get(url, params=params)
    response.raise_for_status()
    return response.json()


def convert_job_config(config_dict: dict[str, Any],
                       raw: bool = False) -> dict[str, Any]:
  if raw or config_dict.get("comparison_mode") != "try":
    return config_dict

  config = PinpointTryJobConfig.from_response_dict(config_dict)
  return config.to_dict()


def print_job_config(job_id: str, raw: bool, full: bool) -> None:
  """Fetches and displays the configuration for a specific Pinpoint job."""
  if full and not raw:
    raise ValueError("The `full` flag requires `raw` to be set.")
  config_dict = fetch_job_config(job_id, full)
  converted_config_dict = convert_job_config(config_dict, raw)
  print(json.dumps(converted_config_dict, indent=2))
