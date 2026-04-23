# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import csv
import datetime as dt
import itertools
import json
import sys
from concurrent.futures import ThreadPoolExecutor
from typing import Any, Final, Mapping

import yaml
from tabulate import tabulate

from crossbench.pinpoint import requests
from crossbench.pinpoint.api import JOB_SHORTEN_URL_TEMPLATE, \
    PINPOINT_JOBS_API_URL, USERINFO_API_URL
from crossbench.pinpoint.format_time import DATETIME_FORMAT, format_time
from crossbench.pinpoint.helper import annotate
from crossbench.pinpoint.list_format import ListFormatEnum
from crossbench.pinpoint.user import UserEnum


def list_jobs(user: UserEnum | str, number: int, truncate: int | None,
              output_format: ListFormatEnum) -> None:
  emails_to_query = _fetch_user_emails(user)

  jobs = []
  with annotate("Fetching jobs"), ThreadPoolExecutor() as executor:
    results = executor.map(lambda email: _fetch_jobs(number, email),
                           emails_to_query)
    jobs = list(itertools.chain.from_iterable(results))

  jobs.sort(key=lambda job: job.get("created", ""), reverse=True)

  if not jobs:
    print("No jobs found.")
    return

  _display_jobs(jobs[:number], output_format, user == UserEnum.ALL, truncate)


def _fetch_user_emails(user: UserEnum | str) -> set[str | None]:
  if user == UserEnum.ME:
    email = _get_user_email()
    username = email.split("@")[0]
    return {email, f"{username}@google.com", f"{username}@chromium.org"}
  if user == UserEnum.ALL:
    return {None}
  return {user}


def _get_user_email() -> str:
  with annotate("Fetching user-email"):
    response = requests.get(USERINFO_API_URL)
    response.raise_for_status()
    return response.json()["email"]


def _fetch_jobs(number: int, email: str | None = None) -> list[dict[str, Any]]:
  jobs = []
  next_cursor = None
  params = {}
  if email:
    params["filter"] = f"user={email}"

  while True:
    if next_cursor:
      params["next_cursor"] = next_cursor

    response = requests.get(PINPOINT_JOBS_API_URL, params=params)
    response.raise_for_status()
    data = response.json()
    jobs.extend(data.get("jobs", []))

    if len(jobs) >= number:
      return jobs[:number]

    next_cursor = data.get("next_cursor")
    if not data.get("next") or not next_cursor:
      break
  return jobs


def _prepare_job_list_data(
    jobs: list[dict[str, Any]],
    all_users: bool) -> tuple[list[str], list[list[Any]]]:
  user_header = []
  if all_users:
    user_header = ["User"]
  headers = [
      "Benchmark", "Config", "Type", *user_header, "Start Time", "Job URL",
      "Status"
  ]
  table_data = []

  for job in jobs:
    created_time = job.get("created")
    if created_time:
      dt_object = dt.datetime.fromisoformat(created_time.replace("Z", "+00:00"))
      created_time = dt_object.strftime(DATETIME_FORMAT)

    user_column = []
    if all_users:
      user_column = [job.get("user", "")]
    row = [
        job.get("arguments", {}).get("benchmark", ""),
        job.get("configuration", ""),
        job.get("comparison_mode", ""),
        *user_column,
        created_time,
        JOB_SHORTEN_URL_TEMPLATE.format(job_id=job.get("job_id", "")),
        job.get("status", ""),
    ]
    table_data.append(row)
  return headers, table_data


def _display_jobs(jobs: list[dict[str, Any]], output_format: ListFormatEnum,
                  all_users: bool, truncate: int | None) -> None:
  match output_format:
    case ListFormatEnum.JSON:
      print(json.dumps(jobs, indent=2))
    case ListFormatEnum.YAML:
      print(yaml.dump(jobs))
    case ListFormatEnum.CSV:
      headers, rows = _prepare_job_list_data(jobs, all_users)
      writer = csv.writer(sys.stdout)
      writer.writerow(headers)
      writer.writerows(rows)
    case ListFormatEnum.TSV:
      headers, rows = _prepare_job_list_data(jobs, all_users)
      writer = csv.writer(sys.stdout, delimiter="\t")
      writer.writerow(headers)
      writer.writerows(rows)
    case ListFormatEnum.TABLE:
      headers, rows = _prepare_job_list_data(jobs, all_users)
      _display_jobs_as_table(headers, rows, truncate)


def _display_jobs_as_table(headers: list[str], rows: list,
                           truncate: int | None) -> None:
  table_data = [[_truncate(cell, truncate) for cell in row] for row in rows]
  url_index = headers.index("Job URL")
  type_index = headers.index("Type")
  time_index = headers.index("Start Time")
  status_index = headers.index("Status")
  for row in table_data:
    row[url_index] = _format_link(row[url_index])
    row[type_index] = _format_type(row[type_index])
    row[time_index] = format_time(row[time_index])
    row[status_index] = _format_status(row[status_index])
  headers[status_index] = "🚦"
  print(tabulate(table_data, headers=headers))


def _format_link(url: str) -> str:
  text = url
  osc8_start = "\x1b]8;;"
  osc8_end = "\x1b\\"
  return f"{osc8_start}{url}{osc8_end}{text}{osc8_start}{osc8_end}"


JOB_TYPE_LOOKUP: Final[Mapping[str, str]] = {
    "performance": "bisect",
    "try": "try",
}


def _format_type(job_type: str) -> str:
  lookup_str = job_type.lower().strip()
  return JOB_TYPE_LOOKUP.get(lookup_str, job_type)


STATUS_EMOJI_LOOKUP: Final[Mapping[str, str]] = {
    "queued": "⌛",
    "running": "🏃",
    "completed": "✅",
    # An extra space is added because this emoji eats a space from the right.
    "cancelled": "⏹️ ",
    "failed": "❌",
}


def _format_status(status: str) -> str:
  lookup_str = status.lower().strip()
  return STATUS_EMOJI_LOOKUP.get(lookup_str, status)


def _truncate(text: str, max_length: int | None = None) -> str:
  text = str(text)
  if max_length and len(text) > max_length:
    return text[:max_length - 3] + "..."
  return text
