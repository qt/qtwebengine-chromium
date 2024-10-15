# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import argparse
import csv
import gzip
import json
import logging
import os
import shutil
import stat
import zipfile
from contextlib import ExitStack
from typing import IO, TYPE_CHECKING, Any, Iterable, List, Optional, Tuple
from urllib.request import urlretrieve

from crossbench import cli_helper, exception
from crossbench import path as pth
from crossbench import plt
from crossbench.browsers.browser_helper import BROWSERS_CACHE
from crossbench.helper.path_finder import TraceProcessorFinder
from crossbench.plt.base import ListCmdArgsT, Platform
from crossbench.probes import metric as cb_metric
from crossbench.probes.probe import Probe, ProbeConfigParser, ProbeContext
from crossbench.probes.results import LocalProbeResult, ProbeResult

if TYPE_CHECKING:
  from crossbench.runner.groups.browsers import BrowsersRunGroup
  from crossbench.runner.run import Run


_QUERIES_DIR = pth.LocalPath(__file__).parent / "queries"
_MODULES_DIR = pth.LocalPath(__file__).parent / "modules/ext"
_METRICS_DIR = pth.LocalPath(__file__).parent / "metrics"

def _is_trace_file(path: pth.LocalPath):
  return path.name.endswith(".trace.pb") or path.name.endswith(
      ".trace.pb.gz") or path.name.endswith(".perf.data")


def _download_trace_processor() -> pth.LocalPath:
  TRACE_PROCESSOR_DOWNLOAD_URL = "https://get.perfetto.dev/trace_processor"
  destination = BROWSERS_CACHE / "trace_processor"
  urlretrieve(TRACE_PROCESSOR_DOWNLOAD_URL, destination)
  st = os.stat(destination)
  os.chmod(destination, st.st_mode | stat.S_IEXEC)
  return destination


# TODO(carlscab): We should use the python API to start a TraceProcessor
# instance and run all queries there, instead of creating one instance per query

class TraceProcessor:
  """
  Helper class to run trace processor queries and metrics and store result in
  csv files.
  """

  def __init__(self,
               platform: Platform,
               trace_processor: pth.LocalPath):
    super().__init__()
    # TODO: add throw parameter.
    self._exceptions = exception.Annotator(throw=True)
    self._platform = platform
    self._trace_processor = trace_processor

  def _build_trace_processor_cmd(self, trace_file: pth.LocalPath,
                                 metric: Optional[str] = None,
                                 query: Optional[str] = None) -> ListCmdArgsT:
    cmd: ListCmdArgsT = [
        self._trace_processor,
        "--metric-extension",
        str(_METRICS_DIR) + "@/ext",
        "--add-sql-module",
        _MODULES_DIR,
    ]
    if metric:
      cmd += ["--metrics-output", "json", "--run-metrics", metric]
    if query:
      cmd += ["--query-file", _QUERIES_DIR / f"{query}.sql"]
    cmd.append(trace_file)

    return cmd

  def run_metrics(self, metrics: Iterable[str], trace_file: pth.LocalPath,
                  out_dir: pth.LocalPath) -> List[pth.LocalPath]:
    if not metrics:
      return []
    json_files: List[pth.LocalPath] = []
    self._platform.mkdir(out_dir)
    for metric in metrics:
      with self._exceptions.capture(f"Running metric: {metric}"):
        safe_filename = pth.safe_filename(metric)
        out_file = out_dir / f"{safe_filename}.json"
        cmd = self._build_trace_processor_cmd(trace_file, metric=metric)
        with out_file.open("x") as f:
          self._platform.sh(*cmd, stdout=f)
        json_files.append(out_file)

    # TODO: consume exception in the caller instead.
    self._exceptions.assert_success()
    return json_files

  def run_queries(self, queries: Iterable[str], trace_file: pth.LocalPath,
                  out_dir: pth.LocalPath) -> List[pth.LocalPath]:
    if not queries:
      return []
    csv_files: List[pth.LocalPath] = []
    self._platform.mkdir(out_dir)
    for query in queries:
      with self._exceptions.capture(f"Running query: {query}"):
        safe_filename = pth.safe_filename(query)
        out_file = out_dir / f"{safe_filename}.csv"
        cmd = self._build_trace_processor_cmd(trace_file, query=query)
        with out_file.open("x") as f:
          self._platform.sh(*cmd, stdout=f)
        csv_files.append(out_file)

    # TODO: consume exception in the caller instead.
    self._exceptions.assert_success()
    return csv_files

  def check_metrics(self, metrics: Iterable[str]) -> None:
    """
    Runs all specified metrics on an empty trace. This will ensure that the
    metrics are correctly defined in trace processor.
    """
    for metric in metrics:
      cmd = self._build_trace_processor_cmd(pth.LocalPath("/dev/null"),
                                            metric=metric)
      process = self._platform.sh(*cmd, capture_output=True, check=False)
      if process.returncode != 0:
        logging.error(
            "Checking metric '%s' failed. Trace processor stderr:\n%s",
            metric, process.stderr.decode('ascii'))
        raise RuntimeError(f"Metric check failed: {metric}")

  def check_queries(self, queries: Iterable[str]) -> None:
    """
    Runs all specified queries on an empty trace. This will ensure that query
    files exist and do not contain SQL syntax errors.
    """
    for query in queries:
      cmd = self._build_trace_processor_cmd(pth.LocalPath("/dev/null"),
                                            query=query)
      process = self._platform.sh(*cmd, capture_output=True, check=False)
      if process.returncode != 0:
        logging.error(
            "Checking query '%s' failed. Trace processor stderr:\n%s",
            query, process.stderr.decode('ascii'))
        raise RuntimeError(f"Query check failed: {query}")


_SOURCE_PROBES: frozenset[str] = frozenset(("perfetto", "tracing", "profiling"))


def parse_probe_name(value: Any) -> str:
  if value in _SOURCE_PROBES:
    return value
  raise argparse.ArgumentTypeError("Unknown trace processor source probe, "
                                   f"choices are: {_SOURCE_PROBES}")


class TraceProcessorProbe(Probe):
  """
  Trace processor probe.
  """

  NAME = "trace_processor"

  @classmethod
  def config_parser(cls) -> ProbeConfigParser:
    parser = super().config_parser()
    parser.add_argument(
        "metrics",
        type=str,
        is_list=True,
        default=tuple(),
        help="Name of metric to be run (can be any metric from Perfetto or "
             "a custom metric from probes/trace_processor/metrics)")
    parser.add_argument(
        "queries",
        type=str,
        is_list=True,
        default=tuple(),
        help="Name of query to be run (under probes/trace_processor/queries)")
    parser.add_argument(
        "probes",
        type=parse_probe_name,
        is_list=True,
        default=tuple(),
        help="Names of probes whose traces need to be processed")
    parser.add_argument(
        "trace_processor_bin",
        type=cli_helper.parse_local_binary_path,
        required=False,
        help="Path to the trace_processor binary")
    return parser

  def __init__(self, metrics: Iterable[str], queries: Iterable[str],
               probes: Iterable[str],
               trace_processor_bin: Optional[pth.LocalPath] = None):
    super().__init__()
    self._metrics = tuple(metrics)
    self._queries = tuple(queries)
    if not probes:
      raise ValueError("Please specify probes to process")
    self._probes = tuple(probes)
    if not trace_processor_bin:
      if tp_chromium_path := TraceProcessorFinder(plt.PLATFORM).path:
        trace_processor_bin = pth.LocalPath(tp_chromium_path)
      else:
        trace_processor_bin = _download_trace_processor()
    self._trace_processor_bin = cli_helper.parse_local_binary_path(
        trace_processor_bin, "trace_processor")

  @property
  def metrics(self) -> Tuple[str, ...]:
    return self._metrics

  @property
  def queries(self) -> Tuple[str, ...]:
    return self._queries

  @property
  def probes(self) -> Tuple[str, ...]:
    return self._probes

  @property
  def trace_processor_bin(self) -> pth.LocalPath:
    return self._trace_processor_bin

  def get_context(self, run: Run) -> TraceProcessorProbeContext:
    return TraceProcessorProbeContext(self, run)

  def _merge_csv(self, group_dir: pth.LocalPath,
                 group: BrowsersRunGroup) -> List[pth.LocalPath]:
    writers: dict[str, csv.DictWriter] = {}
    csv_files: List[pth.LocalPath] = []
    with ExitStack() as stack:
      extra_columns: dict[str, Any] = {}
      for story in group.story_groups:
        extra_columns["browser_label"] = story.browser.label
        for rep in story.repetitions_groups:
          extra_columns["cb_story_name"] = rep.story.name
          for run in rep.runs:
            extra_columns["repetition"] = run.repetition
            for file in run.results[self].csv_list:
              with file.open(newline='') as csv_file:

                def skip_empty_lines(line):
                  return line != '\n'

                reader = csv.DictReader(
                    filter(skip_empty_lines, csv_file),
                    dialect=csv.unix_dialect)
                if not file.name in writers:
                  merged_csv_file = group_dir / file.name
                  csv_files.append(merged_csv_file)
                  f = merged_csv_file.open("x", newline='')
                  stack.enter_context(f)
                  fieldnames = sorted(extra_columns.keys()) + list(
                      reader.fieldnames)
                  cli_helper.parse_unique_sequence(fieldnames, "field names",
                                                   ValueError)
                  w = csv.DictWriter(f, fieldnames=fieldnames)
                  w.writeheader()
                  writers[file.name] = w
                w = writers[file.name]
                for row in reader:
                  row.update(extra_columns)
                  w.writerow(row)
    return csv_files

  def _merge_json(self, group_dir: pth.LocalPath,
                  group: BrowsersRunGroup) -> List[pth.LocalPath]:
    merged_metrics = cb_metric.MetricsMerger()
    for run in group.runs:
      for file in run.results[self].json_list:
        with file.open() as json_file:
          merged_metrics.add(json.load(json_file))

    merged_json_file = group_dir / "metrics.json"
    with merged_json_file.open("x") as f:
      json.dump(merged_metrics.to_json(), f, indent=4)

    return [merged_json_file]

  def merge_browsers(self, group: BrowsersRunGroup) -> ProbeResult:
    group_dir = group.get_local_probe_result_path(self)
    group_dir.mkdir()
    merged_csv = self._merge_csv(group_dir, group)
    merged_json = self._merge_json(group_dir, group)
    return LocalProbeResult(csv=merged_csv, json=merged_json)


class TraceProcessorProbeContext(ProbeContext[TraceProcessorProbe]):

  def __init__(self, probe: TraceProcessorProbe, run: Run) -> None:
    super().__init__(probe, run)
    self._trace_processor = TraceProcessor(run.runner_platform,
                                           probe.trace_processor_bin)

  def get_default_result_path(self) -> pth.RemotePath:
    result_dir = super().get_default_result_path()
    self.runner_platform.mkdir(result_dir)
    return result_dir

  def setup(self) -> None:
    # Before actually running the benchmark, make sure all the metrics and
    # queries exist and don't contain errors.
    self._trace_processor.check_metrics(self.probe.metrics)
    self._trace_processor.check_queries(self.probe.queries)

  def start(self) -> None:
    pass

  def stop(self) -> None:
    pass

  def teardown(self) -> ProbeResult:
    with self.run.actions("TRACE_PROCESSOR: Merging trace files", verbose=True):
      merged_trace = self._merge_trace_files()

    with self.run.actions("TRACE_PROCESSOR: Running queries", verbose=True):
      csv_files = self._trace_processor.run_queries(
          queries=self.probe.queries,
          trace_file=merged_trace,
          out_dir=self.local_result_path)

    with self.run.actions("TRACE_PROCESSOR: Running metrics", verbose=True):
      json_files = self._trace_processor.run_metrics(
          metrics=self.probe.metrics,
          trace_file=merged_trace,
          out_dir=self.local_result_path)

    return LocalProbeResult(file=[merged_trace], csv=csv_files, json=json_files)

  def _merge_trace_files(self) -> pth.LocalPath:
    merged_trace = self.local_result_path / "merged_trace.zip"
    with zipfile.ZipFile(merged_trace, 'w') as zip_file:
      for probe_name in self.probe.probes:
        for f in self.run.results.get_by_name(probe_name).file_list:
          if _is_trace_file(f):
            zip_file.write(f)

    return merged_trace

  def _write_probe_result_traces(self, probe_name: str, output_f: IO) -> None:
    # TODO: implement probe dependencies
    probe_results = self.run.results.get_by_name(probe_name)
    assert probe_results, f"Did not find results for required probe {probe_name}"
    if probe_results.is_empty:
      logging.warn("TRACE_PROCESSOR: No trace files found for %s", probe_name)
      return
    for f in probe_results.file_list:
      if not _is_trace_file(f):
        continue
      if f.suffix == ".gz":
        with gzip.open(f) as input_f:
          shutil.copyfileobj(input_f, output_f)
      else:
        with f.open("rb") as input_f:
          shutil.copyfileobj(input_f, output_f)
