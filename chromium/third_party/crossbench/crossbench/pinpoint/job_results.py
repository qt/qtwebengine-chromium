# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import pathlib
from typing import Any
from urllib.parse import urlparse

from google.cloud import storage
from typing_extensions import override

from crossbench import path as pth
from crossbench import plt
from crossbench.env.base import BaseEnv
from crossbench.pinpoint.helper import annotate
from crossbench.pinpoint.job_config import fetch_job_config
from crossbench.runner.runner import Runner


class PinpointJobResults:

  def __init__(
      self,
      job_id: str,
  ) -> None:
    self.job_id = job_id
    self.data = fetch_job_config(job_id, full=True)
    with annotate("Parsing job results"):
      if self.status.lower() != "completed":
        raise ValueError(f"Job is not completed. Status: {self.status}")

      self.name = f"pinpoint_{self.benchmark}_{self.bot}"
      self.results_url = self.data.get("results_url")
      self.variants = [
          PinpointVeriantResults(v, i)
          for i, v in enumerate(self.data.get("state", []))
      ]

      self.download_index = 0
      self.download_count = 1 if self.results_url else 0
      self.download_count += sum(v.download_count for v in self.variants)

  @property
  def arguments(self) -> dict[str, Any]:
    return self.data.get("arguments", {})

  @property
  def benchmark(self) -> str:
    return self.arguments.get("benchmark", "")

  @property
  def bot(self) -> str:
    return self.arguments.get("configuration", "")

  @property
  def status(self) -> str:
    return self.data["status"]

  def download(self, out_dir: pth.LocalPath) -> None:
    self.download_index = 0
    if self.results_url:
      self._download_from_storage(self.results_url,
                                  out_dir / f"{self.job_id}.html")

    for variant in self.variants:
      variant_dir = out_dir / variant.name
      for attempt in variant.attempts:
        attempt_dir = variant_dir / str(attempt.index + 1)
        attempt_dir.mkdir(parents=True, exist_ok=True)

        if attempt.cas_isolate:
          self._download_cas_isolate(attempt.cas_isolate, attempt_dir)

        for trace_name, trace_url in attempt.perfetto_trace_url_by_name.items():
          self._download_from_storage(trace_url, attempt_dir / trace_name)

  def _next_progress_message(self) -> str:
    self.download_index += 1
    return f"Downloading {self.download_index}/{self.download_count}"

  def _download_cas_isolate(self, isolate: str, out_dir: pth.LocalPath) -> None:
    with annotate(self._next_progress_message()):
      cmd = [
          "cas", "download", "-cas-instance",
          "projects/chrome-swarming/instances/default_instance", "-digest",
          isolate, "-dir",
          str(out_dir)
      ]
      plt.PLATFORM.sh(*cmd)

  def _download_from_storage(self, url: str,
                             output_file: pth.LocalPath) -> None:
    with annotate(self._next_progress_message()):
      parsed_url = urlparse(url)
      path_segments = parsed_url.path.strip("/").split("/", 1)
      if len(path_segments) < 2:
        raise ValueError(f"Invalid GCS URL: {url}")

      bucket_name = path_segments[0]
      blob_name = path_segments[1]

      client = storage.Client()
      bucket = client.bucket(bucket_name)
      blob = bucket.blob(blob_name)

      blob.download_to_filename(str(output_file))



class PinpointVeriantResults:

  def __init__(self, data: dict[str, Any], index: int) -> None:
    self.data = data
    self.index = index
    self.name = self.form_variant_name()
    self.attempts = [
        PinpointAttemptResults(attempt, index)
        for index, attempt in enumerate(data.get("attempts", []))
    ]
    self.download_count = sum(a.download_count for a in self.attempts)

  @property
  def change(self) -> dict[str, Any]:
    return self.data.get("change", {})

  def form_variant_name(self) -> str:
    if label := self.change.get("label"):
      return label

    parts = []
    for commit in self.change.get("commits", []):
      parts.append(commit.get("repository"))
      parts.append(commit.get("commit_position"))

    parts = [str(part) for part in parts if part]
    if parts:
      return "_".join(parts)

    return f"variant_{self.index}"


class PinpointAttemptResults:

  def __init__(self, data: dict[str, Any], index: int) -> None:
    self.data = data
    self.index = index
    self.cas_isolate = self.find_results_isolate()
    self.perfetto_trace_url_by_name = self.find_perfetto_traces()

    self.download_count = len(self.perfetto_trace_url_by_name)
    if self.cas_isolate:
      self.download_count += 1

  @property
  def executions(self) -> list[dict[str, Any]]:
    return self.data.get("executions", [])

  def find_results_isolate(self) -> str | None:
    if len(self.executions) < 2:
      return None

    for details in self.executions[1].get("details", []):
      if details.get("key") == "isolate" and details.get("value"):
        return details.get("value")

    return None

  def find_perfetto_traces(self) -> dict[str, str]:
    url_by_name: dict[str, str] = {}
    if len(self.executions) < 3:
      return url_by_name

    for index, details in enumerate(self.executions[2].get("details", [])):
      if details.get("key") == "trace" and details.get("url"):
        name = str(details.get("value", index))
        if not name.endswith(".pb"):
          name += ".pb"
        url_by_name[name] = details.get("url")
    return url_by_name


class Environment(BaseEnv):

  @override
  def validate(self) -> None:
    pass


def download_results(job_id: str, out_dir: pth.LocalPath | None = None) -> None:
  """Downloads results of a Pinpoint job."""
  Environment(plt.PLATFORM).check_installed(["cas"])
  job_results = PinpointJobResults(job_id)

  out_dir = out_dir or Runner.get_out_dir(
      pathlib.Path.cwd(), suffix=job_results.name)
  out_dir.mkdir(parents=True, exist_ok=True)

  job_results.download(out_dir)
