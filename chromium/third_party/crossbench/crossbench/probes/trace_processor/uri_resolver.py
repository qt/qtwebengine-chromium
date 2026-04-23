# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from typing import TYPE_CHECKING, ClassVar, Final, Iterable

from perfetto.trace_uri_resolver.resolver import TraceUriResolver
from typing_extensions import override

from crossbench.probes.probe_context import ProbeContext
from crossbench.probes.trace_processor.constants import PROBE_NAME

if TYPE_CHECKING:
  from crossbench.probes.trace_processor.context.base import \
      TraceProcessorProbeContext
  from crossbench.runner.run import Run


class CrossbenchTraceUriResolver(TraceUriResolver):
  PREFIX: ClassVar = "crossbench"

  def __init__(self,
               traces: Iterable[Run] | TraceProcessorProbeContext) -> None:
    self._resolved: Final[list[TraceUriResolver.Result]] = self._init_resolved(
        traces)

  def _init_resolved(
      self, traces: Iterable[Run] | TraceProcessorProbeContext
  ) -> list[TraceUriResolver.Result]:
    if isinstance(traces, ProbeContext):
      return self._init_resolved_from_probe_context(traces)
    return self._init_resolved_from_runs(traces)

  def _init_resolved_from_runs(
      self, traces: Iterable[Run]) -> list[TraceUriResolver.Result]:
    resolved: list[TraceUriResolver.Result] = []
    for run in traces:
      trace_result = run.results.get_by_name(PROBE_NAME)
      assert trace_result, f"Missing TraceProcessorProbe result in {run}"
      result = TraceUriResolver.Result(
          trace=str(trace_result.perfetto.absolute()),
          metadata=self._run_metadata(run))
      resolved.append(result)
    return resolved

  def _init_resolved_from_probe_context(
      self, probe_context: TraceProcessorProbeContext
  ) -> list[TraceUriResolver.Result]:
    return [
        TraceUriResolver.Result(
            trace=str(probe_context.merged_trace_path.absolute()),
            metadata=self._run_metadata(probe_context.run))
    ]

  def _run_metadata(self, run: Run) -> dict[str, str]:
    return {
        "cb_browser": run.browser.unique_name,
        "cb_story": run.story.name,
        "cb_temperature": run.temperature,
        "cb_run": str(run.repetition)
    }

  @override
  def resolve(self) -> list[TraceUriResolver.Result]:
    return self._resolved
