# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import zipfile
from typing import TYPE_CHECKING, Optional

from google.protobuf import text_format
from google.protobuf.json_format import MessageToJson
from perfetto.trace_processor.api import TraceProcessor

from crossbench import path as pth
from crossbench.exception import ExceptionAnnotator
from crossbench.helper.cwd import change_cwd
from crossbench.probes.probe_context import ProbeContext
from crossbench.probes.results import EmptyProbeResult, LocalProbeResult, \
    ProbeResult
from crossbench.probes.trace_processor.uri_resolver import \
    CrossbenchTraceUriResolver

if TYPE_CHECKING:
  from crossbench.probes.trace_processor.query_config import \
      TraceProcessorQueryConfig
  from crossbench.probes.trace_processor.trace_processor import \
      TraceProcessorProbe  # noqa: F401


class TraceProcessorProbeContext(ProbeContext["TraceProcessorProbe"]):

  def get_default_result_path(self) -> pth.AnyPath:
    result_dir = super().get_default_result_path()
    self.host_platform.mkdir(result_dir)
    return result_dir

  def setup(self) -> None:
    pass

  def start(self) -> None:
    pass

  def stop(self) -> None:
    pass

  def teardown(self) -> ProbeResult:
    return self._merge_trace_files().merge(self._maybe_run_tp())

  def _merge_trace_files(self) -> LocalProbeResult:
    with self.run.actions("TRACE_PROCESSOR: Merging trace files", verbose=True):
      traces = list(self.run.results.all_traces())
      if len(traces) == 1:
        # Symlink the existing trace to save time and space
        self.host_platform.symlink_or_copy(traces[0], self.merged_trace_path)
      else:
        with zipfile.ZipFile(self.merged_trace_path, "w") as zip_file:
          for f in traces:
            zip_file.write(f, arcname=f.relative_to(self.run.out_dir))
    return LocalProbeResult(perfetto=(self.merged_trace_path,))

  def _maybe_run_tp(self) -> ProbeResult:
    if not self.probe.needs_tp_run:
      return EmptyProbeResult()
    with change_cwd(self.local_result_path), TraceProcessor(
        trace=CrossbenchTraceUriResolver(self),
        config=self.probe.tp_config) as tp:
      with ExceptionAnnotator().annotate() as exceptions:
        query_result = self._run_queries(tp, exceptions)
        metric_result = self._run_metrics(tp, exceptions)
        summary_result = self._summarize_trace(tp)
      result = query_result.merge(metric_result).merge(summary_result)
      return result

  def _run_queries(self, tp: TraceProcessor,
                   exceptions: ExceptionAnnotator) -> LocalProbeResult:

    def run_query(query: TraceProcessorQueryConfig) -> tuple[
          pth.LocalPath, pth.LocalPath]:
      csv_file = self.local_result_path / f"{query.name}.csv"
      json_file = self.local_result_path / f"{query.name}.json"
      df = tp.query(query.sql).as_pandas_dataframe()
      df.to_csv(path_or_buf=csv_file, index=False)
      df.to_json(path_or_buf=json_file, orient="records")
      return csv_file, json_file

    with self.run.actions("TRACE_PROCESSOR: Running queries", verbose=True):
      csv_files = []
      json_files = []
      for query in self.probe.queries:
        with exceptions.capture(f"query: {query}"):
          csv_file, json_file = run_query(query)
          csv_files.append(csv_file)
          json_files.append(json_file)
      return LocalProbeResult(csv=csv_files, json=json_files)

  def _run_metrics(self, tp: TraceProcessor,
                   exceptions: ExceptionAnnotator) -> LocalProbeResult:

    def run_metric(metric: str) -> pth.LocalPath:
      json_file = self.local_result_path / f"{pth.safe_filename(metric)}.json"
      proto = tp.metric([metric])
      assert not json_file.exists(), (
          f"Cannot override previously generated metric {json_file}")
      json_file.write_text(MessageToJson(proto))
      return json_file

    with self.run.actions("TRACE_PROCESSOR: Running metrics", verbose=True):
      json_files = []
      for metric in self.probe.metrics:
        with exceptions.capture(f"metric: {metric}"):
          json_files.append(run_metric(metric))
      return LocalProbeResult(json=json_files)

  def _summarize_trace(self, tp: TraceProcessor) -> ProbeResult:
    if not self.probe.summary_metrics and not self.probe.metric_definitions:
      return EmptyProbeResult()

    with self.run.actions(
        "TRACE_PROCESSOR: Running trace summary", verbose=True):

      # Trace processor interprets an empty list as 'emit no metrics' and
      # 'None' as emit all metrics specified in the metric definitions.
      # When no metric IDs are explicitly given, default to the more
      # sensible option of emitting every metric.
      metric_ids: Optional[list[str]] = None
      if self.probe.summary_metrics:
        metric_ids = list(self.probe.summary_metrics)

      proto_result = tp.trace_summary(
          specs=list(self.probe.metric_definitions), metric_ids=metric_ids)

      proto_file = self.local_result_path / "v2_metrics.pb"
      proto_file.write_bytes(proto_result.SerializeToString())

      textproto_file = self.local_result_path / "v2_metrics.textproto"
      textproto_file.write_bytes(text_format.MessageToBytes(proto_result))

      return LocalProbeResult(file=[proto_file, textproto_file])

  @property
  def merged_trace_path(self) -> pth.LocalPath:
    return self.local_result_path / "merged_trace.zip"

  @property
  def symbolized_trace_path(self) -> pth.LocalPath:
    return self.local_result_path / "symbolized_trace_path.zip"
