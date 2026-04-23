# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import atexit
import io
import logging
import subprocess
from functools import cached_property
from typing import TYPE_CHECKING, Iterable, Optional, Self, Type, cast

from typing_extensions import override

from crossbench.browsers.chromium_based.chromium_based import ChromiumBased
from crossbench.helper import wait
from crossbench.parse import NumberParser
from crossbench.probes.chromium_probe import ChromiumProbe
from crossbench.probes.probe_context import ProbeContext
from crossbench.probes.probe_error import ProbeIncompatibleBrowser, \
    ProbeValidationError
from crossbench.probes.result_location import ResultLocation
from crossbench.probes.results import EmptyProbeResult, ProbeResult

if TYPE_CHECKING:
  import crossbench.path as pth
  from crossbench.browsers.browser import Browser
  from crossbench.env.runner_env import RunnerEnv
  from crossbench.plt.types import ListCmdArgs
  from crossbench.probes.probe import ProbeConfigParser, ProbeKeyT
  from crossbench.runner.run import Run


class EtmProbe(ChromiumProbe):
  """
  Probe for collecting instruction traces using CoreSight Embedded Trace
  Macrocell with perf.

  This probe is compatible with Linux and Android devices that have CoreSight
  enabled in the kernel and the `perf` tool available.

  Currently only targets RendererMain and pins it to a single cpu
  """

  NAME = "etm"
  RESULT_LOCATION = ResultLocation.BROWSER

  @classmethod
  @override
  def config_parser(cls) -> ProbeConfigParser[Self]:
    parser = super().config_parser()
    parser.add_argument(
        "cpu",
        type=NumberParser.positive_zero_int,
        required=True,
        help=(
            "The cpu that RendererMain will be pinned to along and sampled from."
        ),
    )
    parser.add_argument(
        "aux_buffer_size",
        type=NumberParser.power_of_two_with_unit,
        default="4M",
        help=("Set aux buffer size."
              "Need to be power of 2 and page size aligned."
              "Used memory size is (buffer_size * (cpu_count + 1))."
              "Default is 4M."),
    )
    parser.add_argument(
        "record_timestamp",
        type=bool,
        default=False,
        help=("Generate timestamp packets in ETM stream."),
    )
    parser.add_argument(
        "record_cycles",
        type=bool,
        default=False,
        help=("Generate cycle count packets in ETM stream."),
    )
    parser.add_argument(
        "cycle_threshold",
        type=NumberParser.positive_int,
        default=None,
        help=("Set cycle count counter threshold for ETM cycle count packets."),
    )
    parser.add_argument(
        "flush_interval",
        type=NumberParser.positive_int,
        default=None,
        help=("Set the interval between ETM data flushes from the ETR buffer"
              "to the perf event buffer (in milliseconds). Default is 100 ms."),
    )
    return parser

  def __init__(
      self,
      cpu: int,
      aux_buffer_size: Optional[str] = None,
      record_timestamp: bool = False,
      record_cycles: bool = False,
      cycle_threshold: Optional[int] = None,
      flush_interval: Optional[int] = None,
  ) -> None:
    super().__init__()
    self._cpu: int = cpu
    self._aux_buffer_size: Optional[str] = aux_buffer_size
    self._record_timestamp: bool = record_timestamp
    self._record_cycles: bool = record_cycles
    self._cycle_threshold: Optional[int] = cycle_threshold
    self._flush_interval: Optional[int] = flush_interval

  @property
  def key(self) -> ProbeKeyT:
    return super().key + (
        ("cpu", self._cpu),
        ("aux_buffer_size", self._aux_buffer_size),
        ("record_timestamp", self._record_timestamp),
        ("record_cycles", self._record_cycles),
        ("cycle_threshold", self._cycle_threshold),
        ("flush_interval", self._flush_interval),
    )

  @property
  def cpu(self) -> int:
    return self._cpu

  @property
  def aux_buffer_size(self) -> Optional[str]:
    return self._aux_buffer_size

  @property
  def record_timestamp(self) -> bool:
    return self._record_timestamp

  @property
  def record_cycles(self) -> bool:
    return self._record_cycles

  @property
  def cycle_threshold(self) -> Optional[int]:
    return self._cycle_threshold

  @property
  def flush_interval(self) -> Optional[int]:
    return self._flush_interval

  @override
  def validate_env(self, env: RunnerEnv) -> None:
    super().validate_env(env)
    if not env.platform.which("simpleperf"):
      raise ProbeValidationError(self, "simpleperf not found")
    try:
      result: str = env.platform.sh_stdout("simpleperf", "list", "cs-etm")
      if "unknown event" in result or not result:
        raise ProbeValidationError(self, "cs_etm event not found")
    except subprocess.CalledProcessError as exc:
      raise ProbeValidationError(self,
                                 "Failed to query for ETM support.") from exc

  @override
  def validate_browser(self, env: RunnerEnv, browser: Browser) -> None:
    super().validate_browser(env, browser)
    if not browser.platform.is_android:
      raise ProbeIncompatibleBrowser(self, browser, "Only supported on Android")

  @override
  def attach(self, browser: Browser) -> None:
    super().attach(browser)
    chromium = cast(ChromiumBased, browser)
    chromium.flags.enable_benchmarking_api()

  @override
  def get_context_cls(self) -> Type[EtmProbeContext]:
    return EtmProbeContext


class EtmProbeContext(ProbeContext[EtmProbe]):

  def __init__(self, probe: EtmProbe, run: Run) -> None:
    super().__init__(probe, run)
    self._etm_process: Optional[subprocess.Popen] = None

  @cached_property
  def _renderer_tid(self) -> int:
    assert self._story_ready, (
        "Fetching renderer PID/TID before the story is loaded could lead to "
        "the wrong PID/TID being used. This should never happen TM!")
    renderer_main_tid: Optional[int] = None
    with self.run.actions("Get Renderer Main TID") as actions:
      renderer_main_tid = actions.js(
          "return chrome?.benchmarking?.getRendererMainTid?.();")
    if not renderer_main_tid:
      raise ValueError("Could not get Renderer Main TID from browser.")
    return renderer_main_tid

  def _start_etm(self) -> None:
    command_line: ListCmdArgs = [
        "simpleperf",
        "record",
        "--clockid",
        "monotonic",
        "-e",
        "cs-etm",
        "-t",
        str(self._renderer_tid),
        "--cpu",
        str(self.probe.cpu),
    ]

    if self.probe.aux_buffer_size:
      command_line.extend(["--aux-buffer-size", self.probe.aux_buffer_size])
    if self.probe.record_timestamp:
      command_line.append("--record-timestamp")
    if self.probe.record_cycles:
      command_line.append("--record-cycles")
    if self.probe.cycle_threshold is not None:
      command_line.extend(
          ["--cycle-threshold",
           str(self.probe.cycle_threshold)])
    if flush_interval := self.probe.flush_interval:
      command_line.extend(["--etm-flush-interval", str(flush_interval)])

    command_line.extend(["-o", self.result_path.as_posix()])

    logging.info("Starting etm with command line: %s.", command_line)
    self._etm_process = self.browser_platform.popen(
        *command_line, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    wait.sleep(1)
    if self._etm_process.poll():
      error_msg: str = ""
      if stdout := self._etm_process.stdout:
        if isinstance(stdout, io.BufferedReader):
          error_msg = stdout.read().decode("utf-8")
          logging.error(error_msg)
      raise ValueError(f"Unable to start etm. {error_msg}")
    atexit.register(self.stop_process)

  def _get_etm_pids(self) -> list[int]:
    etm_pids: list[int] = []
    for process in self.browser_platform.processes():
      if process["name"] == "simpleperf":
        etm_pids.append(process["pid"])
    return etm_pids

  def _stop_existing_etm(self) -> None:
    for etm_pid in self._get_etm_pids():
      logging.warning("Terminating existing etm process: %d.", etm_pid)
      self.browser_platform.terminate(etm_pid)

  def _cpu_mask(self, cpus: Iterable) -> str:
    assert max(cpus) < 32, "Cpu index too high"
    mask: int = 0
    for cpu in cpus:
      mask |= 1 << cpu
    return f"{mask:x}"

  def _pin_renderer_main_core(self, cpu: int) -> None:
    self.browser_platform.sh("taskset", "-p", self._cpu_mask([cpu]),
                             str(self._renderer_tid))

  def get_default_result_path(self) -> pth.AnyPath:
    result_dir = super().get_default_result_path()
    self.browser_platform.mkdir(result_dir)
    return result_dir / "etm.perf.data"

  def setup(self) -> None:
    assert (self.browser.platform.is_android
           ), f"Expected Android platform, found {type(self.browser.platform)}."
    assert (self.browser.attributes().is_chromium_based
           ), f"Expected Chromium-based browser, found {type(self.browser)}."
    self._stop_existing_etm()

  @override
  def start(self) -> None:
    pass

  @override
  def stop(self) -> None:
    self.stop_process()

  @override
  def start_story_run(self) -> None:
    self._story_ready = True
    self._pin_renderer_main_core(self.probe.cpu)
    self._start_etm()

  def stop_process(self) -> None:
    if self._etm_process:
      self.browser_platform.terminate_gracefully(
          self._etm_process,
          timeout=60,
          signal=self.browser_platform.signals.SIGINT,
      )
    self._etm_process = None

  def teardown(self) -> ProbeResult:
    if not self.browser_platform.is_file(self.result_path):
      logging.warning("simpleperf ETM data file was not created or is empty.")
      return EmptyProbeResult()
    return self.browser_result(perfetto=(self.result_path,))
