# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import json
import re
from urllib.parse import urlparse

from crossbench.pinpoint import requests


def resolve_patch(patch: str) -> str:
  change_id, patchset = _extract_crrev_change_id_and_patchset(patch)
  if change_id:
    return _revision_to_url(change_id, patchset)

  if _is_url(patch):
    return patch

  raise ValueError(f"Invalid patch: {patch}")


def _extract_crrev_change_id_and_patchset(
    patch: str) -> tuple[str | None, str | None]:
  patch = patch.strip().lower()
  match = re.fullmatch(
      r"(?:https?://)?(?:crrev(?:\.com)?/)?(?:c/)?(\d+)(?:/(\d+))?", patch)
  if match:
    return match.group(1), match.group(2)
  return None, None


def _is_url(patch: str) -> bool:
  try:
    result = urlparse(patch)
    return all([result.scheme in ("http", "https"), result.netloc])
  except ValueError:
    return False


def _revision_to_url(change_id: str, patchset: str | None = None) -> str:
  chromium_review_url = "https://chromium-review.googlesource.com"

  api_response = requests.get(chromium_review_url + f"/changes/{change_id}")
  api_response.raise_for_status()

  # Skip the protective prefix ")]}'"
  content = api_response.text[api_response.text.find("{"):]
  data = json.loads(content)
  project = data["project"]
  url = chromium_review_url + f"/c/{project}/+/{change_id}"
  return url + (f"/{patchset}" if patchset else "")
