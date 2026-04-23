# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import abc
import atexit
import datetime as dt
import logging
import subprocess
from typing import TYPE_CHECKING, Final

import google.protobuf.text_format as proto_text_format

from crossbench.parse import NumberParser
from crossbench.probes.perfetto.constants import PERFETTO_CONFIG_NAME, \
    PERFETTO_TRACE_NAME
from crossbench.probes.probe_context import ProbeContext
from crossbench.probes.results import LocalProbeResult, ProbeResult

if TYPE_CHECKING:
  from crossbench import path as pth
  from crossbench.plt.types import TupleCmdArgs
  from crossbench.probes.perfetto.perfetto import PerfettoProbe
  from crossbench.runner.run import Run

PERFETTO_STOP_TIMEOUT: Final[dt.timedelta] = dt.timedelta(seconds=30)


class PerfettoProbeContext(
    ProbeContext["PerfettoProbe"], metaclass=abc.ABCMeta):

  def __init__(self, probe: PerfettoProbe, run: Run) -> None:
    self._file_prefix: Final[str] = dt.datetime.now().strftime(
        "%Y-%m-%d_%H%M%S")
    super().__init__(probe, run)
    self._host_config_file: Final[pth.LocalPath] = (
        run.out_dir / PERFETTO_CONFIG_NAME)
    self._perfetto_pid: int | None = None

  def setup(self) -> None:
    assert self._perfetto_pid is None
    for p in self.browser_platform.processes():
      if p["name"] == "perfetto":
        logging.warning("PERFETTO: killing existing session pid: %s", p["pid"])
        self.browser_platform.terminate(p["pid"])
    self._setup_validate_bin()
    self._setup_push_perfetto_config()
    if self.probe.trace_browser_startup:
      self._start_perfetto()

  def _setup_validate_bin(self) -> None:
    binary = self.perfetto_cmd[0]
    if not self.browser_platform.which(binary):
      raise ValueError(
          f"{repr(binary)} cannot be found on {self.browser_platform}")

  def _setup_push_perfetto_config(self) -> None:
    self.host_platform.write_text(
        self._host_config_file,
        proto_text_format.MessageToString(self.probe.trace_config))
    if not self.probe.config_via_stdin:
      self.browser_platform.push(self._host_config_file,
                                 self.get_browser_config_path())

  @abc.abstractmethod
  def get_browser_config_path(self) -> pth.AnyPath:
    pass

  @abc.abstractmethod
  def get_default_result_path(self) -> pth.AnyPath:
    pass

  @property
  def perfetto_cmd(self) -> TupleCmdArgs:
    return (self.probe.perfetto_bin,)

  def start(self) -> None:
    if self.probe.trace_browser_startup:
      if not self._perfetto_pid:
        raise RuntimeError("Perfetto was not started")
      return
    self._start_perfetto()
    self.browser.performance_mark("probe-perfetto-start")

  def stop(self) -> None:
    self.browser.performance_mark("probe-perfetto-stop")
    logging.info("PERFETTO: stopping")
    if not self._perfetto_pid:
      raise RuntimeError("Perfetto was not started")
    self._stop_perfetto()

  def _start_perfetto(self) -> None:
    logging.info("PERFETTO: starting")
    cmd: TupleCmdArgs = self.perfetto_cmd + (
        "--background",
        "--config",
        "-" if self.probe.config_via_stdin else self.get_browser_config_path(),
        "--txt",
        "--out",
        self.result_path,
    )
    try:
      if self.probe.config_via_stdin:
        with self._host_config_file.open() as f:
          proc = self.browser_platform.sh(*cmd, stdin=f, capture_output=True)
      else:
        proc = self.browser_platform.sh(*cmd, capture_output=True)
    except subprocess.CalledProcessError as e:
      logging.error("perfetto command failed with stderr: %s",
                    e.stderr.decode(encoding="utf-8"))
      raise

    self._perfetto_pid = NumberParser.positive_int(
        proc.stdout.decode("utf-8").rstrip(), "perfetto pid")
    atexit.register(self._stop_perfetto)

  def _stop_perfetto(self) -> None:
    if not self._perfetto_pid:
      return
    atexit.unregister(self._stop_perfetto)
    # TODO(cbruni): replace with terminate_gracefully
    self.browser_platform.terminate(self._perfetto_pid)
    try:
      for _ in self.run.wait_range(1,
                                   PERFETTO_STOP_TIMEOUT).wait_with_backoff():
        if not self.browser_platform.process_info(self._perfetto_pid):
          break
    except TimeoutError:
      logging.error(
          "perfetto process did not stop after %s"
          "The trace might be incomplete.", PERFETTO_STOP_TIMEOUT)
    self._perfetto_pid = None

  def teardown(self) -> ProbeResult:
    try:
      return self._transfer_results()
    finally:
      if self.browser_platform.is_remote:
        self._cleanup_remote_perfetto_files()

  def _transfer_results(self) -> ProbeResult:
    browser_result = self.browser_result(file=[self.result_path])
    local_result_file = browser_result.file
    assert local_result_file.is_file(), (
        f"Could not copy perfetto results: {local_result_file}")
    renamed_result_file = local_result_file.with_name(PERFETTO_TRACE_NAME)
    self.host_platform.rename(local_result_file, renamed_result_file)

    self.host_platform.sh("gzip", renamed_result_file)
    renamed_result_file = renamed_result_file.with_suffix(
        f"{local_result_file.suffix}.gz")
    assert renamed_result_file.is_file(), (
        f"Could not compress {renamed_result_file}")

    return LocalProbeResult(perfetto=(renamed_result_file,))

  def _cleanup_remote_perfetto_files(self) -> None:
    # Especially on android, the perfetto files are not in the default tmp dir.
    self.browser_platform.rm(self.result_path, missing_ok=True)
    self.browser_platform.rm(self.get_browser_config_path(), missing_ok=True)
