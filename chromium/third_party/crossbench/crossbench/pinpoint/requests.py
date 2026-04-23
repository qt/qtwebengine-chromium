# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import json

import requests

from crossbench.pinpoint import auth


def get(url: str, **kwargs) -> requests.Response:
  return _method("GET", url, **kwargs)


def post(url: str, **kwargs) -> requests.Response:
  return _method("POST", url, **kwargs)


class ServerError(requests.exceptions.HTTPError):

  def __init__(self, error: requests.exceptions.HTTPError) -> None:
    error_message = ""
    try:
      data = error.response.json()
      if error_text := data.get("error"):
        error_message = f"\n{error_text}"
      else:
        error_message = f"\n{data}"
    except json.JSONDecodeError:
      pass
    super().__init__(str(error) + error_message, response=error.response)


def _method(method: str, url: str, **kwargs) -> requests.Response:
  response = auth.get_auth_session().request(method, url, **kwargs)
  _raise_for_status(response)
  return response


def _raise_for_status(response: requests.Response) -> None:
  try:
    response.raise_for_status()
  except requests.exceptions.HTTPError as e:
    raise ServerError(e) from e
