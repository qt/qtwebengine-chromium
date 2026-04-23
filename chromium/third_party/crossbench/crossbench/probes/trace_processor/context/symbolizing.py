# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import logging
import os
import zipfile
from typing import TYPE_CHECKING

from typing_extensions import override

from crossbench.plt.base import SubprocessError
from crossbench.probes.profiling.system_profiling import ProfilingProbe
from crossbench.probes.trace_processor.context.base import \
    TraceProcessorProbeContext

if TYPE_CHECKING:
  from crossbench.probes.results import LocalProbeResult


class TraceProcessorSymbolizingProbeContext(TraceProcessorProbeContext):

  @property
  def should_symbolize_profile(self) -> bool:
    if not self.probe.symbolize_profile:
      return False
    return self.run.has_probe_context(ProfilingProbe)

  @override
  def _merge_trace_files(self) -> LocalProbeResult:
    result = super()._merge_trace_files()
    if self.should_symbolize_profile:
      return self._symbolize_profile(result)
    return result

  def _symbolize_profile(self, result: LocalProbeResult) -> LocalProbeResult:
    llvm_symbolizer_bin = self.probe.llvm_symbolizer_bin
    if not llvm_symbolizer_bin:
      logging.error("Could not find llvm-symbolizer binary")
      return result
    traceconv_bin = self.probe.traceconv_bin
    if not traceconv_bin:
      logging.error("Could not find traceconv binary")
      return result
    merged_file = result.get("zip")
    symbols_result = self.local_result_path / "symbols.pb"
    env = {
        "PERFETTO_SYMBOLIZER_MODE": "index",
        "PERFETTO_BINARY_PATH": str(self.run.browser.app_path.parent),
        **self.host_platform.environ,
    }
    env["PATH"] = (os.pathsep).join(
        (str(llvm_symbolizer_bin.parent), env.get("PATH", "")))
    try:
      self.host_platform.sh(
          traceconv_bin, "symbolize", merged_file, symbols_result, env=env)
    except SubprocessError as e:
      logging.error("Symbolization failed: %s", e)
    if not self.host_platform.exists(symbols_result):
      # Figure out why this regularly fails
      logging.error("Could not generate symbols file: %s", symbols_result)
      return result
    with zipfile.ZipFile(self.symbolized_trace_path, "w") as zip_file:
      for f in (*result.perfetto_list, symbols_result):
        zip_file.write(f, arcname=f.relative_to(self.run.out_dir))
    return self.local_result(perfetto=(self.symbolized_trace_path,))
