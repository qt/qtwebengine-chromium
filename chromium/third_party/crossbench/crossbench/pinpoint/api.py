# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from typing import Final

PINPOINT_API_URL_BASE: Final[
    str] = "https://pinpoint-dot-chromeperf.appspot.com/api"
PINPOINT_JOBS_API_URL: Final[str] = f"{PINPOINT_API_URL_BASE}/jobs"
PINPOINT_JOB_API_URL_TEMPLATE: Final[
    str] = f"{PINPOINT_API_URL_BASE}/job/{{job_id}}"
PINPOINT_START_JOB_API_URL: Final[str] = f"{PINPOINT_API_URL_BASE}/new"
PINPOINT_CANCEL_JOB_API_URL: Final[str] = f"{PINPOINT_API_URL_BASE}/job/cancel"
PINPOINT_CONFIG_API_URL: Final[str] = f"{PINPOINT_API_URL_BASE}/config"
PINPOINT_BUILDS_API_URL_TEMPLATE: Final[
    str] = f"{PINPOINT_API_URL_BASE}/builds/{{bot}}"

CHROMEPERF_API_URL_BASE: Final[str] = "https://chromeperf.appspot.com/api"
CHROMEPERF_TEST_SUITES_API_URL: Final[
    str] = f"{CHROMEPERF_API_URL_BASE}/test_suites"
CHROMEPERF_DESCRIBE_API_URL: Final[str] = f"{CHROMEPERF_API_URL_BASE}/describe"

USERINFO_API_URL: Final[str] = "https://www.googleapis.com/oauth2/v3/userinfo"
JOB_SHORTEN_URL_TEMPLATE: Final[str] = "http://go/j_/{job_id}"
