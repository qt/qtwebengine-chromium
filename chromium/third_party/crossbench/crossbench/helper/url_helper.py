# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import datetime as dt
import logging
import urllib.parse as urlparse
from typing import Any, Dict, Iterator, Mapping, Optional

import requests

from crossbench.helper import wait
from crossbench.runner.timing import AnyTime

DEFAULT_REQUEST_TIMEOUT = dt.timedelta(seconds=10)

RequestException = requests.RequestException
HTTPError = requests.HTTPError
ConnectionError = requests.ConnectionError  # pylint: disable=redefined-builtin

Response = requests.Response


def get(url: str,
        timeout: AnyTime = DEFAULT_REQUEST_TIMEOUT,
        retry: int = 0,
        verbose: bool = True) -> requests.Response:
  max_request_count = retry + 1
  request_timeout_seconds = to_seconds(timeout) / max_request_count
  for i in _retry(retry):
    try:
      if verbose:
        logging.debug("GET: url: %s", url)
      response = requests.get(url, timeout=request_timeout_seconds)
      response.raise_for_status()
      return response
    except requests.RequestException as e:
      if i < retry:
        if verbose:
          logging.warning("GET request failed url=%s, retrying: %s", url, e)
        continue
      if verbose:
        logging.error("GET request failed url=%s", url)
      raise e
  raise RuntimeError("Could not complete request")


def post(url: str,
         body_json: Optional[Any] = None,
         headers: Optional[Mapping[str, str]] = None,
         timeout: AnyTime = DEFAULT_REQUEST_TIMEOUT,
         retry: int = 0,
         verbose: bool = True) -> requests.Response:
  max_request_count = retry + 1
  request_timeout_seconds = to_seconds(timeout) / max_request_count
  for i in _retry(retry):
    try:
      response = requests.post(
          url, headers=headers, json=body_json, timeout=request_timeout_seconds)
      response.raise_for_status()
      return response
    except requests.RequestException as e:
      if i < retry:
        if verbose:
          logging.warning("POST request failed url=%s retrying: %s", url, e)
        continue
      if verbose:
        logging.error("POST request failed url=%s", url)
      raise e
  raise RuntimeError("Could not complete request")


def to_seconds(delta: AnyTime) -> float:
  if isinstance(delta, dt.timedelta):
    return delta.total_seconds()
  return delta


def _retry(retry: int) -> Iterator[int]:
  max_iterations = retry + 1
  wait_range = wait.WaitRange(min=1, max_iterations=max_iterations)
  for i, _, _ in wait_range.wait_with_backoff():
    yield i


def update_url_query(url: str, query_params: Dict[str, str]) -> str:
  parsed_url = urlparse.urlparse(url)
  query = dict(urlparse.parse_qsl(parsed_url.query))
  query.update(query_params)
  parsed_url = parsed_url._replace(query=urlparse.urlencode(query, doseq=True))
  return parsed_url.geturl()


def quote(value: str) -> str:
  return urlparse.quote(value)
