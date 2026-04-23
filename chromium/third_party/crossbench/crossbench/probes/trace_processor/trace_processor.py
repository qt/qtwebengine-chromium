# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import collections
import json
import logging
from typing import TYPE_CHECKING, ClassVar, Final, Iterable, Optional, Self, \
    Type

import pandas as pd
from google.protobuf.json_format import MessageToJson
from perfetto.batch_trace_processor.api import BatchTraceProcessor, \
    BatchTraceProcessorConfig
from perfetto.trace_processor.api import TraceProcessor, TraceProcessorConfig
from perfetto.trace_uri_resolver.path import PathUriResolver
from perfetto.trace_uri_resolver.registry import ResolverRegistry
from typing_extensions import override

from crossbench import path as pth
from crossbench import plt
from crossbench.exception import ExceptionAnnotator
from crossbench.helper.cwd import change_cwd
from crossbench.helper.path_finder import LlvmSymbolizerFinder, \
    TraceconvFinder, TraceProcessorFinder
from crossbench.parse import ObjectParser
from crossbench.probes.metric import MetricsMerger
from crossbench.probes.probe import Probe, ProbeConfigParser, ProbePriority
from crossbench.probes.results import LocalProbeResult, ProbeResult
from crossbench.probes.trace_processor.constants import MODULES_DIR, \
    PROBE_NAME, QUERIES_DIR
from crossbench.probes.trace_processor.context.base import \
    TraceProcessorProbeContext
from crossbench.probes.trace_processor.context.symbolizing import \
    TraceProcessorSymbolizingProbeContext
from crossbench.probes.trace_processor.query_config import \
    TraceProcessorQueryConfig
from crossbench.probes.trace_processor.uri_resolver import \
    CrossbenchTraceUriResolver

if TYPE_CHECKING:
  from crossbench.env.runner_env import RunnerEnv
  from crossbench.runner.groups.browsers import BrowsersRunGroup
  from crossbench.runner.run import Run
  from crossbench.types import JsonDict


class TraceProcessorProbe(Probe):
  """
  Trace processor probe.
  """

  NAME: ClassVar = PROBE_NAME
  PRIORITY: ClassVar = ProbePriority.TRACE_PROCESSOR

  @classmethod
  @override
  def config_parser(cls) -> ProbeConfigParser[Self]:
    parser = super().config_parser()
    parser.add_argument(
        "batch",
        type=bool,
        default=False,
        help="Run queries in batch mode when all the test runs are done. This "
        "can considerably reduce the run time at the expense of higher "
        "memory usage (all traces will be loaded into memory at the same "
        "time)")
    parser.add_argument(
        "metrics",
        type=str,
        is_list=True,
        default=(),
        help="Name of metric to be run (can be any metric from Perfetto)")
    parser.add_argument(
        "metric_definitions",
        type=ObjectParser.str_or_file_contents,
        is_list=True,
        default=(),
        help=("Textproto for perfetto metrics v2 definition files. "
              "Can be inline textproto or a path to a .textproto file."))
    parser.add_argument(
        "summary_metrics",
        type=str,
        is_list=True,
        default=(),
        help=("Additional metrics to only include in the trace summary. "
              "Includes all of <metrics>. These can be v2 metrics if the "
              "corresponding metric definition is supplied."))
    parser.add_argument(
        "queries",
        type=TraceProcessorQueryConfig,
        is_list=True,
        default=(),
        help="Name of query to be run (under probes/trace_processor/queries) "
        "or { name: str, sql: str } containing the name and SQL query to run")
    parser.add_argument(
        "symbolize_profile",
        type=ObjectParser.bool,
        default=True,
        help="Auto symbolize data from system profiles for "
        "locally compiled browsers.")
    parser.add_argument(
        "module_paths",
        type=pth.LocalPath,
        is_list=True,
        default=(),
        help="Additional paths to include as trace processor modules.")
    parser.add_argument(
        "trace_processor_bin",
        aliases=("trace_processor",),
        type=plt.PLATFORM.parse_local_binary_path,
        help="Path to the trace_processor binary")
    parser.add_argument(
        "traceconv_bin",
        aliases=("traceconv",),
        type=plt.PLATFORM.parse_local_binary_path,
        help="Path to the perfetto traceconv binary")
    parser.add_argument(
        "llvm_symbolizer_bin",
        aliases=("llvm_symbolizer",),
        type=plt.PLATFORM.parse_local_binary_path,
        help="Path to the llvm-symbolizer binary")
    parser.add_argument(
        "dev_features",
        aliases=("dev",),
        type=ObjectParser.bool,
        default=True,
        help=("Enables trace_processor dev features via the --dev flags. "
              "Enabled by default."))
    return parser

  def __init__(
      self,
      batch: bool,
      metric_definitions: Iterable[str],
      summary_metrics: Iterable[str],
      metrics: Iterable[str],
      queries: Iterable[TraceProcessorQueryConfig],
      symbolize_profile: bool,
      module_paths: Iterable[pth.LocalPath],
      trace_processor_bin: Optional[pth.LocalPath] = None,
      traceconv_bin: Optional[pth.LocalPath] = None,
      llvm_symbolizer_bin: Optional[pth.LocalPath] = None,
      dev_features: bool = True,
  ) -> None:
    super().__init__()
    self._platform: Final[plt.Platform] = plt.PLATFORM
    self._batch: Final[bool] = batch
    self._dev_features: Final[bool] = dev_features
    self._metrics: Final[tuple[str, ...]] = tuple(metrics)
    self._metric_definitions: Final[tuple[str, ...]] = tuple(metric_definitions)
    self._summary_metrics: Final[tuple[
        str, ...]] = tuple(metrics) + tuple(summary_metrics)
    ObjectParser.unique_sequence([query.name for query in queries],
                                 name="query names")
    self._queries: Final[tuple[TraceProcessorQueryConfig, ...]] = tuple(queries)
    self._symbolize_profile: Final[bool] = symbolize_profile
    self._module_paths: Final[tuple[pth.LocalPath,
                                    ...]] = (MODULES_DIR,) + tuple(module_paths)
    self._trace_processor_bin: Final[
        pth.LocalPath
        | None] = TraceProcessorFinder.local_binary(trace_processor_bin)
    self._traceconv_bin: Final[pth.LocalPath
                               | None] = TraceconvFinder.local_binary(
                                   traceconv_bin)
    self._llvm_symbolizer_bin: Final[
        pth.LocalPath
        | None] = LlvmSymbolizerFinder.local_binary(llvm_symbolizer_bin)

  @property
  def batch(self) -> bool:
    return self._batch

  @property
  def metrics(self) -> tuple[str, ...]:
    return self._metrics

  @property
  def queries(self) -> tuple[TraceProcessorQueryConfig, ...]:
    return self._queries

  @property
  def metric_definitions(self) -> tuple[str, ...]:
    return self._metric_definitions

  @property
  def summary_metrics(self) -> tuple[str, ...]:
    return self._summary_metrics

  @property
  def module_paths(self) -> tuple[pth.LocalPath, ...]:
    return self._module_paths

  @property
  def has_work(self) -> bool:
    return len(self._queries) != 0 or len(self._metrics) != 0 or len(
        self._summary_metrics) != 0 or len(self._metric_definitions) != 0

  @property
  def needs_tp_run(self) -> bool:
    return (not self.batch) and self.has_work

  @property
  def needs_btp_run(self) -> bool:
    return self._batch and self.has_work

  @property
  def trace_processor_bin(self) -> pth.LocalPath | None:
    return self._trace_processor_bin

  @property
  def traceconv_bin(self) -> pth.LocalPath | None:
    return self._traceconv_bin

  @property
  def llvm_symbolizer_bin(self) -> pth.LocalPath | None:
    return self._llvm_symbolizer_bin

  @property
  def symbolize_profile(self) -> bool:
    return self._symbolize_profile

  @property
  def tp_config(self) -> TraceProcessorConfig:
    extra_flags: list[str] = []
    if self._dev_features:
      extra_flags.append("--dev")
    is_debug_logging = logging.getLogger().isEnabledFor(logging.DEBUG)

    for module_path in self.module_paths:
      extra_flags.append("--add-sql-module")
      extra_flags.append(str(module_path))

    return TraceProcessorConfig(
        bin_path=self.trace_processor_bin,
        ingest_ftrace_in_raw=True,
        verbose=is_debug_logging,
        resolver_registry=ResolverRegistry(
            resolvers=[CrossbenchTraceUriResolver, PathUriResolver]),
        load_timeout=10,
        extra_flags=extra_flags)

  @override
  def get_context_cls(self) -> Type[TraceProcessorProbeContext]:
    # TODO: enable on linux and android
    if self._platform.is_macos:
      return TraceProcessorSymbolizingProbeContext
    return TraceProcessorProbeContext

  @override
  def validate_env(self, env: RunnerEnv) -> None:
    super().validate_env(env)
    with ExceptionAnnotator().annotate(
        "Validating metrics and queries") as exceptions:
      self._validate_metrics_and_queries(exceptions)

  def _validate_metrics_and_queries(self,
                                    exceptions: ExceptionAnnotator) -> None:
    """
    Runs all metrics and queries on an empty trace. This will ensure that they
    are correctly defined in trace processor.
    """
    with TraceProcessor(trace="/dev/null", config=self.tp_config) as tp:
      for metric in self.metrics:
        with exceptions.capture(f"metric: {metric!r}"):
          tp.metric([metric])
      for query in self.queries:
        with exceptions.capture(f"query: {query.name!r}"):
          tp.query(query.sql)

      if summary_metrics := self.summary_metrics:
        with exceptions.capture("summary metrics:"):
          tp.trace_summary(
              specs=list(self.metric_definitions),
              metric_ids=list(summary_metrics))

  def _add_cb_columns(self, df: pd.DataFrame, run: Run) -> pd.DataFrame:
    df["cb_browser"] = run.browser.unique_name
    df["cb_story"] = run.story.name
    df["cb_temperature"] = run.temperature
    df["cb_run"] = run.repetition
    return df

  def _aggregate_results_by_query(
      self, runs: Iterable[Run]) -> dict[str, pd.DataFrame]:
    res: dict[str, pd.DataFrame] = {}
    for run in runs:
      for file in run.results[self].csv_list:
        df = pd.read_csv(file)
        df = self._add_cb_columns(df, run)
        if file.stem in res:
          res[file.stem] = pd.concat([res[file.stem], df])
        else:
          res[file.stem] = df

    return res

  def _merge_json(self, runs: Iterable[Run]) -> dict[str, JsonDict]:
    merged_metrics: dict[str,
                         MetricsMerger] = collections.defaultdict(MetricsMerger)
    for run in runs:
      for file_path in run.results[self].json_list:
        with file_path.open() as json_file:
          merged_metrics[file_path.stem].add(json.load(json_file))

    return {
        metric_name: merged.to_json()
        for metric_name, merged in merged_metrics.items()
    }

  @override
  def merge_browsers(self, group: BrowsersRunGroup) -> ProbeResult:
    if self.needs_btp_run:
      return self._run_btp(group)

    return self._merge_browser_files(group)

  def _merge_browser_files(self, group: BrowsersRunGroup) -> LocalProbeResult:
    group_dir = group.get_local_probe_result_path(self)
    group_dir.mkdir()
    csv_files = []
    json_files = []
    for query, df in self._aggregate_results_by_query(group.runs).items():
      csv_file = group_dir / f"{pth.safe_filename(query)}.csv"
      df.to_csv(path_or_buf=csv_file, index=False)
      csv_files.append(csv_file)
    for metric, data in self._merge_json(group.runs).items():
      json_file = group_dir / f"{pth.safe_filename(metric)}.json"
      with json_file.open("x") as f:
        json.dump(data, f, indent=4)
      json_files.append(json_file)
    return LocalProbeResult(csv=csv_files, json=json_files)

  def _run_btp(self, group: BrowsersRunGroup) -> LocalProbeResult:
    group_dir: pth.LocalPath = group.get_local_probe_result_path(self)
    group_dir.mkdir()
    btp_config = BatchTraceProcessorConfig(tp_config=self.tp_config)

    with change_cwd(group_dir), BatchTraceProcessor(
        traces=CrossbenchTraceUriResolver(group.runs), config=btp_config
    ) as btp, ExceptionAnnotator().annotate() as exceptions:
      csv_files, json_files = self._run_btp_queries(btp, group_dir, exceptions)
      json_files += self._run_btp_metrics(btp, group_dir, exceptions)
    return LocalProbeResult(csv=csv_files, json=json_files)

  def _run_btp_queries(self, btp: BatchTraceProcessor, group_dir: pth.LocalPath,
                       exceptions: ExceptionAnnotator) -> tuple[
                           list[pth.LocalPath], list[pth.LocalPath]]:

    def run_query(query: TraceProcessorQueryConfig) -> tuple[
        pth.LocalPath, pth.LocalPath]:
      csv_file = group_dir / f"{query.name}.csv"
      json_file = group_dir / f"{query.name}.json"
      df = btp.query_and_flatten(query.sql)
      df.to_csv(path_or_buf=csv_file, index=False)
      # Remove metadata columns to allow for passing the data for JSON result
      # through the MetricsMerger.
      df.drop(
          columns=["cb_browser", "cb_story", "cb_temperature", "cb_run"],
          inplace=True)
      records = df.to_dict(orient="records")
      merged = MetricsMerger()
      merged.add(records)
      with json_file.open("x") as f:
        json.dump(merged.to_json(), f, indent=4)
      return csv_file, json_file

    csv_files: list[pth.LocalPath] = []
    json_files: list[pth.LocalPath] = []
    with exceptions.annotate("Running queries"):
      for query in self.queries:
        with exceptions.annotate(f"query: {query.name}"):
          csv_file, json_file = run_query(query)
          csv_files.append(csv_file)
          json_files.append(json_file)
    return csv_files, json_files

  def _run_btp_metrics(self, btp: BatchTraceProcessor, group_dir: pth.LocalPath,
                       exceptions: ExceptionAnnotator) -> list[pth.LocalPath]:

    def run_metric(metric: str) -> pth.LocalPath:
      json_file = group_dir / f"{pth.safe_filename(metric)}.json"
      protos = btp.metric([metric])
      with json_file.open("x") as f:
        for p in protos:
          f.write(MessageToJson(p))
      return json_file

    json_files: list[pth.LocalPath] = []
    with exceptions.annotate("Running metrics"):
      for metric in self.metrics:
        with exceptions.annotate(f"metric: {metric}"):
          json_files.append(run_metric(metric))
    return json_files

  @override
  def log_browsers_result(self, group: BrowsersRunGroup) -> None:
    logging.info("-" * 80)
    logging.critical("TraceProcessor merged traces:")
    for run in group.runs:
      logging.critical("  - %s", run.results[self].perfetto)

__all__ = [
    "TraceProcessorProbe",
    "QUERIES_DIR",
    "MODULES_DIR",
]
